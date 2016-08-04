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

int make_server_socket(const char *ip, int port)
{
	int server_socket;

	if(-1 == (server_socket = socket(AF_INET, SOCK_STREAM, 0)))
		myErr("server socket failed");

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

#endif