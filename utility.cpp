#include <iostream>
#include <vector>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>

#include "config.h"

using namespace std;

/*
	ユーザーの命令を分析します

	chat|we|hello -> weに"hello"を送信します
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
	epollのEdge　Triggerモードに、sockｆｄは非閉塞
*/
void set_unblocking(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

/*
	ファイルを受信するとき、sockｆｄを閉塞にします
*/
void set_blocking(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
}

/*
	ｆｄをepollfdに追加して、監視し始めます
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
	fdをepollfdから削除して、監視しない
*/
void epfd_del(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;			// fd read enable

	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
}

/*
	SignalのHandler関数を指定します
*/
void sig_add(int sig, void (* handler)(int), bool restart = true)
{
	struct sigaction sa;
	bzero(&sa, sizeof(sa));
	sa.sa_handler = handler;
	// signal()の登録効果は一回だけですので、sigactionを利用します
	CHK_ERROR(sigaction(sig, &sa, NULL));
}