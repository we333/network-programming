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
#define PORT 		(11115)
#define MAX_EPOLL_EVENT_NUM 	(4096)
#define	ERROR_EXIT	{cout<<__FUNCTION__<<": "<<__LINE__<<" line"<<endl; perror(" "); exit(-1);}
#define CHK_ERROR(x)	{if(0 > (x))		ERROR_EXIT;}
#define CHK_NULL(x)		{if(NULL == (x))	ERROR_EXIT;}
#define TOKEN 		("|")
#define FILE_PATH 	("static/")
#define MAX_CHILD_PROCESS_NUM	(1)
#define READ 		(0)
#define WRITE		(1)
#define USER_MAX_NUM 	(65535)

#define REPLY_SUCCESS	("success\n")
#define REPLY_NORESULTS ("noresults\n")
#define REPLY_FAILED	("fail\n")
#define REPLY_INVALID	("invalid\n")
#define REPLY_UNDEFINED	("undefined\n")

int epfd;			// epoll fd
epoll_event events[MAX_EPOLL_EVENT_NUM];
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

vector<string> split(char *buf)
{
	char *p;
	char *ptr = strtok_r(buf, TOKEN, &p);

	vector<string> vs;
	while(ptr)
	{
		vs.push_back(ptr);
		ptr = strtok_r(NULL, TOKEN, &p);
	}

	return vs;
}

int make_client_socket(const char *ip, int port)
{
	int client_socket;

	if(-1 == (client_socket = socket(AF_INET, SOCK_STREAM, 0)))
		ERROR_EXIT("client socket failed");

	sockaddr_in addr;
	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if(-1 == connect(client_socket, (sockaddr *)&addr, sizeof(sockaddr_in)))
		ERROR_EXIT("connect failed");

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

void epfd_add(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;			// fd read enable & edge trigger mode
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

	sa.sa_handler = handler;
	if(restart)
	;//	sa.sa_flags |= SA_RESTART;
	CHK_ERROR(sigfillset(&sa.sa_mask));
	CHK_ERROR(sigaction(sig, &sa, NULL));
}

#endif
