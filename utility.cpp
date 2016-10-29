#include <iostream>
#include <vector>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>

#include "config.h"

using namespace std;

/*
Summary : segment parameter from string
	chat|we|hello -->
		para1 = chat
		para2 = we
		para3 = hello
Parameters:
	buf : received string from client
Return : 
	segmented string
*/
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

/*
Summary : set file descriptor as unblocking mode
Parameters:
	fd : file descriptor
Return : 
*/
void set_unblocking(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

/*
Summary : set file descriptor as blocking mode
Parameters:
	fd : file descriptor
Return : 
*/
void set_blocking(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
}

/*
Summary : add file descriptor into epoll's monitor queue
Parameters:
	epollfd : epoll file descriptor
	fd : file descriptor
Return : 
*/
void epfd_add(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;			// fd read enable & edge trigger mode
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

	set_unblocking(fd);
}

/*
Summary : delete file descriptor from epoll's monitor queue
Parameters:
	epollfd : epoll file descriptor
	fd : file descriptor
Return : 
*/
void epfd_del(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;			// fd read enable

	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
}

/*
Summary : register signal event
Parameters:
	sig : signal ID
	handler : signal handler
Return : 
*/
void sig_add(int sig, void (* handler)(int))
{
	struct sigaction sa;
	bzero(&sa, sizeof(sa));
	sa.sa_handler = handler;
	// signal()の登録効果は一回だけですので、sigactionを利用します
	CHK_ERROR(sigaction(sig, &sa, NULL));
}