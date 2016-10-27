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
#define IP 			("192.168.80.131")
#define PORT 		(11115)

#define MAX_CHILD_PROCESS_NUM	(5)
#define READ 					(0)
#define WRITE					(1)
#define MAX_EPOLL_EVENT_NUM 	(4096)
#define USER_MAX_NUM 			(65535)

#define TOKEN 			("|")
#define FILE_PATH 		("static/")
#define REPLY_SUCCESS	("success\n")
#define REPLY_NORESULTS ("noresults\n")
#define REPLY_FAILED	("fail\n")
#define REPLY_INVALID	("invalid\n")
#define REPLY_UNDEFINED	("undefined\n")

#define	ERROR_EXIT	{cout<<__FUNCTION__<<": "<<__LINE__<<" line"<<endl; perror(" "); exit(-1);}
#define CHK_ERROR(x)	{if(0 > (x))		ERROR_EXIT;}
#define CHK_NULL(x)		{if(NULL == (x))	ERROR_EXIT;}

int epfd;			// epoll fd
epoll_event events[MAX_EPOLL_EVENT_NUM];
list<int> cs;		// save client_fd

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

	CHK_ERROR(sigaction(sig, &sa, NULL));
}

#endif
