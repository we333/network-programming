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

int main(int ac, char *av[])
{
	signal(SIGCHLD, child_waiter);
	cs.clear();

	int listener = make_server_socket(IP, PORT);
	int epfd, event_cnt;

	if(-1 == (epfd = epoll_create(EPOLL_SIZE)))
		myErr("create epoll failed");
	struct epoll_event events[EPOLL_SIZE];
	epfd_add(epfd, listener, true);

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
			else
			{
				char msg[BUFSIZ], buf[BUFSIZ];
				int len = recv(sockfd, buf, BUFSIZ, 0);
				if(0 > len)
				{
					myErr("recv failed");
				}
				else if(0 == len)	
				{
					cs.remove(sockfd);
					close(sockfd);
					epfd_del(epfd, sockfd);		// 不再监视exit的用户fd
					cout<<"client exit"<<endl;
				}
				else
				{
					cout<<"recevie from client :"<<buf<<endl;
					sprintf(msg, "client %d said %s", sockfd, buf);
					list<int>::iterator it;
			        for(it = cs.begin(); it != cs.end(); it++)
						if(*it != sockfd)
							{if(-1 == send(*it, msg, BUFSIZ, 0)) myErr("send to client failed");}
				}
			}
		}
	}

	close(listener);
	close(epfd);
	return 0;
}