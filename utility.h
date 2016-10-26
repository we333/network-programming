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
#include <pthread.h>
#include <vector>
#include <string.h>

using namespace std;

//#define IP 			("192.168.68.211")	// ubuntu
#define IP 			("192.168.68.134")		// centOS
//#define IP 			("210.129.54.191")
#define PORT 		(11111)
#define EPOLL_MAX_EVENT 	(4096)
#define	myErr		{cout<<__FUNCTION__<<": "<<__LINE__<<" line"<<endl; perror(" "); exit(-1);}
#define Try(x)		{if(-1 == (x)) myErr;}
#define TOKEN 		("|")
#define FILE_PATH 	("static/")
#define CHILD_PROCESS_NUM	(5)
#define READ 		(0)
#define WRITE		(0)
#define USER_MAX_NUM 	(65535)

int epfd;			// epoll fd
list<int> cs;		// save client_fd

void child_waiter(int num)
{
    int status;
    waitpid(-1, &status, WNOHANG);   
    if (!WIFEXITED(status))
    	cout<<"child waiter failed"<<endl;
    else
    	cout<<"child over"<<endl;
}

vector<string> split(char *buf, const char *token)
{
	vector<string> vs;
	char *p;
	char *ptr = strtok_r(buf, token, &p);

	while(p)
	{
		vs.push_back(p);
		p = strtok_r(NULL, token, &p);
	}
	
	return vs;
}

int make_server_socket(const char *ip, int port)
{
	int yes = 1;
	int server_socket;

	if(-1 == (server_socket = socket(AF_INET, SOCK_STREAM, 0)))
		myErr("server socket failed");

	if (-1 == setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))	// reuse port
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

void sig_add(int sig, void (* handler)(int), bool restart = true)
{
	struct sigaction sa;
	bzero(&sa, sizeof(sa));
	if(restart)
		sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	sigaction(sig, &sa, NULL);
}

#endif
