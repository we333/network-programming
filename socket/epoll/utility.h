#ifndef UTILITY_H_INCLUDED
#define UTILITY_H_INCLUDED

#include <iostream>
#include <list>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

#define IP ("127.0.0.1")
#define PORT (11112)
#define EPOLL_SIZE (4096)
#define	myErr(x)	{perror(x); exit(-1);}

list<int> cs;

void child_waiter(int num)
{
    int status;
/*    pid_t pid;
    while((pid = waitpid(-1, &status, WNOHANG)))
        cout<<"child <<"<<pid<<" terminated"<<endl;
*/
    waitpid(-1, &status, WNOHANG);   
    if (!WIFEXITED(status))
    	cout<<"child waiter failed"<<endl;
    else
    	cout<<"child over"<<endl;
}

int make_server_socket(const char *ip, int port)
{
	int yes = 1;
	int server_socket;

	if(-1 == (server_socket = socket(AF_INET, SOCK_STREAM, 0)))
		myErr("server socket failed");

	// 使linux允许server断开后,绑定的port可以立即被再次使用
	if (-1 == setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))
        myErr("setsockopt failed");

	sockaddr_in addr;
	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if(-1 == bind(server_socket, (sockaddr *)&addr, sizeof(sockaddr_in)))
		myErr("bind failed");

	if(-1 == listen(server_socket, 5))
		myErr("listen failed");

	return server_socket;
}

int make_client_socket(const char *ip, int port)
{
	int client_socket;

	if(-1 == (client_socket = socket(AF_INET, SOCK_STREAM, 0)))
		myErr("client socket failed");

	sockaddr_in addr;
	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if(-1 == connect(client_socket, (sockaddr *)&addr, sizeof(sockaddr_in)))
		myErr("connect failed");

	return client_socket;
}

void set_noblocking(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void epfd_add(int epollfd, int fd, bool et)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;			// fd read enable
	if(et)
		event.events |= EPOLLET;	// edge triggered	
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

	set_noblocking(fd);
}

void epfd_del(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;			// fd read enable

	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
}

void handler_connect(int epfd, int sockfd)
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

void handler_message(int epfd, int sockfd)
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

#endif
