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

#include "utility.h"
#include "sqllib.h"

void client_connect(int sockfd);
void client_message(int sockfd);
void message_route(int sockfd, vector<string> vs);

int main(int ac, char *av[])
{
	signal(SIGCHLD, child_waiter);
	cs.clear();		// 清理客户信息

	int listener = make_server_socket(IP, PORT);
	int event_cnt;

	if(-1 == (epfd = epoll_create(EPOLL_SIZE)))
		myErr("create epoll failed");
	epoll_event events[EPOLL_SIZE];
	epfd_add(epfd, listener, true);		// epoll中注册服务器fd

	while(1)
	{
		// -1: epoll_wait will return when events occured
		// 0: epoll_wait will return immediately, deprecated
		if(-1 == (event_cnt = epoll_wait(epfd, events, EPOLL_SIZE, -1)))
			myErr("epoll wait failed");

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

	if(-1 == (clientfd = accept(sockfd, (sockaddr *)&client_address, &addrlen)))
		myErr("accept failed");
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
		myErr("recv failed");
	}
	else if(0 == len)	
	{
		cs.remove(sockfd);
		close(sockfd);
		epfd_del(epfd, sockfd);		// 不再监视离开的用户的fd
		cout<<"client exit"<<endl;
	}
	else
	{/*
		cout<<"recevie from client :"<<buf<<endl;
		sprintf(msg, "client %d said %s", sockfd, buf);
		list<int>::iterator it;
        for(it = cs.begin(); it != cs.end(); it++)
			if(*it != sockfd)
				{if(-1 == send(*it, msg, BUFSIZ, 0)) myErr("send to client failed");}
		*/
		message_route(sockfd, split(buf));
	}
}

void message_route(int sockfd, vector<string> vs)
{
	if(0 == vs.size())
		myErr("message NULL");

	for(int i = 0; i < vs.size(); i++)
		cout<<vs[i]<<endl;

	if("login" == vs[0])
	{
		login_info usr;
		usr.name = vs[1];
		usr.pwd = vs[2];
		if(wesql.Login(usr))
		{
			if(-1 == send(sockfd, "Success", BUFSIZ, 0))
				myErr("login send success failed");
		}
		else
		{
			if(-1 == send(sockfd, "Fail", BUFSIZ, 0))
				myErr("login send fail failed");
		}
	}
	else if("register" == vs[0])
	{
		register_info usr;
		usr.name = vs[1];
		usr.pwd = vs[2];
		usr.email = vs[3];
		usr.sex = vs[4];
		usr.age = vs[5];
		if(wesql.Register(usr))
		{
			if(-1 == send(sockfd, "Success", BUFSIZ, 0))
				myErr("register send success failed");
		}
		else
		{
			if(-1 == send(sockfd, "Fail", BUFSIZ, 0))
				myErr("register send fail failed");
		}
	}
	else if("chat" == vs[0])
	{
		cout<<"welcome to chat room"<<endl;
	}
	else
		;
}