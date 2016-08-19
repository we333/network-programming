/*
	1.注册SIGNAL用于子进程回收
	2.绑定ip和port,获得listener
	3.epoll-->添加监听listener
	4.while(1)
		等待epoll返回监听事件数量(非阻塞),然后轮询处理各个fd
		|
		|--服务器自身fd-->处理新连接的客户(添加eproll_event,添加客户数组)
		|
		|--客户fd-->recv
					|
					|--> =0,客户断开连接-->关闭fd,客户数组剔除,epoll_event剔除
					|
					|--> >0,接收客户的消息并处理(fork)
*/

/*
完成功能:
	1.login
	2.chat|name|msg 聊天指定发送对象	(用户登录时,fd写入mysql)
	3.chat|all|msg  聊天内容发往所有在线用户
	4.用户退出时,fd清空为-1

待导入功能:
	1.登录后,数据库更新登录状态
	2.客户通过命令获取当前在线用户名单
	3.是否使用多线程
	4.用于聊天的fd保存在内存中,而不是每次chat都去读数据库

Bug:
	register的id自增问题
*/

#include "utility.h"
#include "sqllib.h"

void client_connect(int sockfd);
void client_message(int sockfd);
void message_route(int sockfd, vector<string> vs);
	void Login(int sockfd, vector<string> vs);
	void Register(int sockfd, vector<string> vs);
	void Chat(int sockfd, vector<string> vs);
	void Search(int sockfd, vector<string> vs);
	void Upload(int sockfd, vector<string> vs);

typedef struct
{
	string cmd;
	void (*func)(int, vector<string>);
}Service;

Service service[] = 
{
	{"login", 		Login},
	{"register", 	Register},
	{"chat",		Chat},
	{"search",		Search},
	{"upload", 		Upload},
};

int main(int ac, char *av[])
{
	signal(SIGCHLD, child_waiter);
	cs.clear();		// 清理客户信息

	int listener = make_server_socket(IP, PORT);
	int event_cnt;

	Try(epfd = epoll_create(EPOLL_SIZE))

	epoll_event events[EPOLL_SIZE];
	epfd_add(epfd, listener, true);		// epoll中注册服务器fd

	while(1)
	{
		// -1: epoll_wait will return when events occured
		// 0: epoll_wait will return immediately, deprecated
		Try((event_cnt = epoll_wait(epfd, events, EPOLL_SIZE, -1)))

		for(int i = 0; i < event_cnt; i++)
		{
			int sockfd = events[i].data.fd;

			if(sockfd == listener)
				client_connect(sockfd);
			else
				client_message(sockfd);	
		}
	}

	close(listener);
	close(epfd);
	return 0;
}

void client_connect(int sockfd)
{
	int clientfd;
	sockaddr_in client_address;
	socklen_t addrlen = sizeof(sockaddr_in);

	Try((clientfd = accept(sockfd, (sockaddr *)&client_address, &addrlen)))
	cs.push_back(clientfd);
	epfd_add(epfd, clientfd, true);

	printf("client connection from: %s : % d(IP : port), clientfd = %d \n",
    		inet_ntoa(client_address.sin_addr),
    		ntohs(client_address.sin_port),
    		clientfd);
}

void client_message(int sockfd)
{
	char msg[BUFSIZ], buf[BUFSIZ];
	bzero(msg, BUFSIZ); bzero(buf, BUFSIZ);
	int len = recv(sockfd, buf, BUFSIZ, 0);
	if(0 > len)
	{
		myErr;
	}
	else if(0 == len)
	{
		cout<<"******client exit******"<<endl;
		cs.remove(sockfd);
		close(sockfd);
		epfd_del(epfd, sockfd);		// 不再监视离开的用户的fd
		// 一旦下线,用于聊天的addr也清空为-1
		wesql.ClearAddr(sockfd);
	}
	else
	{
		message_route(sockfd, split(buf));
	}
}

void message_route(int sockfd, vector<string> vs)
{
	if(0 == vs.size())
		myErr;

	for(int i = 0; i < sizeof(service)/sizeof(service[0]); i++)
		if(service[i].cmd == vs[0])
			{service[i].func(sockfd, vs); return;}	// command route
	
	Try(send(sockfd, "undefined cmd", BUFSIZ, 0))
}

void Login(int sockfd, vector<string> vs)
{
	login_info usr;
	usr.name = vs[1];
	usr.pwd = vs[2];
	if(wesql.Login(usr, sockfd))		// 传入clientfd更新数据库信息,用于聊天
		Try(send(sockfd, "Success", BUFSIZ, 0))
	else
		Try(send(sockfd, "Fail", BUFSIZ, 0))
}

void Register(int sockfd, vector<string> vs)
{
	register_info usr;
	usr.name = vs[1];
	usr.pwd = vs[2];
	usr.email = vs[3];
	usr.sex = vs[4];
	usr.age = vs[5];
	if(wesql.Register(usr))
		Try(send(sockfd, "Success", BUFSIZ, 0))
	else
		Try(send(sockfd, "Fail", BUFSIZ, 0))
}

void Chat(int sockfd, vector<string> vs)
{
	// 为了能发消息给vs[1],读取name=vs[1]的对象此刻数据库中存放的fd
	string from = wesql.FindNameFromAddr(sockfd);
	string msg = "<" + from + ">:" + vs[2];
	if("all" == vs[1])
	{
		list<int>::iterator it;
    	for(it = cs.begin(); it != cs.end(); it++)
			if(sockfd != *it)
				Try(send(*it, msg.c_str(), BUFSIZ, 0))// 广播不要发给自己
	}
	else
	{
		int to = atoi(wesql.FindAddrFromName(vs[1]).c_str());
		Try(send(to, msg.c_str(), BUFSIZ, 0))
	}
}

void Search(int sockfd, vector<string> vs)
{
	vector<string> name;
	search_info info;
	info.time = vs[1];
	info.start = vs[2];
	info.end = vs[3];

	name = wesql.Search(info);

	vector<string>::iterator it;
	for(it = name.begin(); it != name.end(); it++)	// bug 由于client的epoll监听是同一事件,连续send两次消息,client也只能处理一次消息
		Try(send(sockfd, (*it).c_str(), BUFSIZ, 0));
}

void Upload(int sockfd, vector<string> vs)
{
	carpool_info info;
	info.name = vs[1];
	info.time = vs[2];
	info.start = vs[3];
	info.end = vs[4];
	info.price = vs[5];
	info.seat = vs[6];
	info.comment = vs[7];
	if(wesql.Upload(info))
		Try(send(sockfd, "Success", BUFSIZ, 0))
	else
		Try(send(sockfd, "Fail", BUFSIZ, 0))
}