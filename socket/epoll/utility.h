#ifndef UTILITY_H_INCLUDED
#define UTILITY_H_INCLUDED

#include <iostream>
#include <list>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>

using namespace std;

#define IP 			("192.168.11.22")
//#define IP 			("localhost")
//#define IP 			("210.129.54.191")
//#define IP 				("127.0.0.1")
#define PORT 		(11111)
#define EPOLL_SIZE 	(4096)
#define	myErr		{cout<<__FUNCTION__<<": "<<__LINE__<<" line"<<endl; perror(" "); exit(-1);}
#define Try(x)		{if(-1 == (x)) myErr;}
#define SPLIT 		("|")
#define FILE_PATH 	("static/")

int epfd;			// epoll fd
list<int> cs;		// 保存client_fd

void child_waiter(int num)
{
    int status;
    waitpid(-1, &status, WNOHANG);   
    if (!WIFEXITED(status))
    	cout<<"child waiter failed"<<endl;
    else
    	cout<<"child over"<<endl;
}

vector<string> split(char *buf)
{
	vector<string> vs;
	const char *d = SPLIT;
	char *p = strtok(buf, d);

	while(p)
	{
		vs.push_back(p);
		p = strtok(NULL, d);
	}
	
	return vs;
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

	cout<<"ip = "<<ip<<endl;

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

void set_unblocking(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void set_blocking(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
}

void epfd_add(int epollfd, int fd, bool et)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;			// fd read enable
	if(et)
		event.events |= EPOLLET;	// edge triggered	
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

	set_unblocking(fd);
}

void epfd_del(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;			// fd read enable

	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
}

#endif
