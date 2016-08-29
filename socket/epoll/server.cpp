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
	void Booking(int sockfd, vector<string> vs);
	void Send_file_to_client(int sockfd, vector<string> vs);
	void Recv_file_from_client(int sockfd, vector<string> vs);
void client_reply(int sockfd, string reply);

typedef struct
{
	string cmd;
	int size;	// Protocol: define the number of parameter in each command
	void (*func)(int, vector<string>);
}Service;

Service sv[] = 
{
	{"login", 		3,	Login 					},
	{"register", 	4,	Register 				},
	{"chat",		3,	Chat 					},
	{"search",		4,	Search 					},
	{"upload", 		8,	Upload 					},
	{"booking",		5,	Booking 				},
	{"pullfile",	3,	Send_file_to_client		},
	{"pushfile",	3,	Recv_file_from_client	},
};

int main(int ac, char *av[])
{
	signal(SIGCHLD, child_waiter);
	cs.clear();		// 清理客户信息

	int listener = make_server_socket(av[1], PORT);
	int event_cnt;

	Try(epfd = epoll_create(EPOLL_SIZE))

	epoll_event events[EPOLL_SIZE];
	epfd_add(epfd, listener, true);		// epoll中注册服务器fd

//	daemon(0,0);

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
	int len;
	char buf[BUFSIZ]; bzero(buf, BUFSIZ);
	
	Try(len = recv(sockfd, buf, BUFSIZ, 0))
	if(0 == len)
	{
		cout<<"******client exit******"<<endl;
		cs.remove(sockfd);
		close(sockfd);
		epfd_del(epfd, sockfd);		// 不再监视离开的用户的fd
		// 一旦下线,用于聊天的addr也清空为-1
		wesql.ClearAddr(sockfd);
	}
	else
		message_route(sockfd, split(buf));

}

void message_route(int sockfd, vector<string> vs)
{
	for(int i = 0; i < vs.size(); i++)
		cout<<"param["<<i<<"] = "<<vs[i]<<endl;
	cout<<"---------------------"<<endl;

	for(int i = 0; i < sizeof(sv)/sizeof(sv[0]); i++)
	{	
		if(sv[i].cmd == vs[0])
		{
			if(vs.size() != sv[i].size)			// number of parameter is incorrect
				{client_reply(sockfd, "invalid\n");	return;}
			{sv[i].func(sockfd, vs); return;}	// valid command, action it
		}
	}
	client_reply(sockfd, "undefined\n");		// undefined command
}

void Login(int sockfd, vector<string> vs)
{
	login_info usr;
	usr.name = vs[1];
	usr.pwd = vs[2];
	if(wesql.Login(usr, sockfd))		// 传入clientfd更新数据库信息,用于聊天
		client_reply(sockfd, "success\n");
	else
		client_reply(sockfd, "fail\n");
}

void Register(int sockfd, vector<string> vs)
{
	register_info usr;
	usr.name = vs[1];
	usr.pwd = vs[2];
	usr.email = vs[3];
	if(wesql.Register(usr))
		client_reply(sockfd, "success\n");
	else
		client_reply(sockfd, "fail\n");
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
				client_reply(*it, msg.c_str());	// 广播不要发给自己
	}
	else
	{
		int to = atoi(wesql.FindAddrFromName(vs[1]).c_str());
		client_reply(to, msg.c_str());
	}
}

void Search(int sockfd, vector<string> vs)
{
	vector<string> db_res;
	search_info info;
	info.date = vs[1];
	info.start = vs[2];
	info.end = vs[3];

	db_res = wesql.Search(info);

	if(0 == db_res.size())
		client_reply(sockfd, "noresults\n");

	string msg;
	vector<string>::iterator it;
	for(it = db_res.begin(); it != db_res.end(); it++)	// bug 由于client的epoll监听是同一事件,连续send两次消息,client也只能处理一次消息
		msg += *it + '|';
	msg += '\n';		// for Android recv, add '\n' at end of string !!!!

	client_reply(sockfd, msg.c_str());
}

void Upload(int sockfd, vector<string> vs)
{
	carpool_info info;
	info.name = vs[1];
	info.date = vs[2];
	info.start = vs[3];
	info.end = vs[4];
	info.price = vs[5];
	info.seat = vs[6];
	info.comment = vs[7];
	if(wesql.Upload(info))
		client_reply(sockfd, "success\n");
	else
		client_reply(sockfd, "fail\n");
}

void Booking(int sockfd, vector<string> vs)
{
	booking_info info;
	info.name = vs[1];
	info.date = vs[2];
	info.start = vs[3];
	info.end = vs[4];

	if(wesql.Booking(info))
		client_reply(sockfd, "success\n");
	else
		client_reply(sockfd, "fail\n");
}

void Send_file_to_client(int sockfd, vector<string> vs)
{
	string filename = FILE_PATH + vs[1] + "." + vs[2];
	int fd = open(filename.c_str(), O_RDONLY);

	struct stat stat_buf;
	fstat(fd, &stat_buf);

	usleep(1);
	Try(sendfile(sockfd, fd, NULL, stat_buf.st_size))
}

void Recv_file_from_client(int sockfd, vector<string> vs)
{
	set_blocking(sockfd);

	FILE *f;
	string filename = FILE_PATH + vs[1] + "." + vs[2];
	char buf[BUFSIZ]; bzero(buf, BUFSIZ);

	if(NULL == (f = fopen(filename.c_str(), "wb+")))
		myErr;

	int len = 0;
	while(len = recv(sockfd, buf, BUFSIZ, 0))
	{
		cout<<"recv "<<len<<" bytes"<<endl;
		if(len < 0)	myErr;
		if(len > fwrite(buf, sizeof(char), len, f))
			myErr;
	}
	cout<<"recv file finished"<<endl;
	cout<<"---------------------"<<endl;
	fclose(f);

	set_unblocking(sockfd);
}

void client_reply(int sockfd, string reply)
{
	Try(send(sockfd, reply.c_str(), BUFSIZ, 0))
}