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
#define IP 			("192.168.68.134")
#define PORT 		(11111)

#define MAX_CHILD_PROCESS_NUM	(5)		// SubProcessの数
#define READ 					(0)		// Pipeを読み込み
#define WRITE					(1)		//　Pipeを書き込み
#define MAX_EPOLL_EVENT_NUM 	(4096)	// epoll監視イベントの数

#define TOKEN 			("|")			// 命令集は param1|param2|param3の形です
#define FILE_PATH 		("static/")		// ファイルを保存する場所

// ユーザーへ返事の内容
#define REPLY_SUCCESS	("success\n")
#define REPLY_NORESULTS ("noresults\n")
#define REPLY_FAILED	("fail\n")
#define REPLY_INVALID	("invalid\n")
#define REPLY_UNDEFINED	("undefined\n")
#define REPLY_UNLOGINED	("user unlogin\n")

// コードを簡潔するために、エラーチェックをマクロにします
#define	ERROR_EXIT	{cout<<__FUNCTION__<<": "<<__LINE__<<" line"<<endl; perror(" "); exit(-1);}
#define CHK_ERROR(x)	{if(0 > (x))		ERROR_EXIT;}
#define CHK_NULL(x)		{if(NULL == (x))	ERROR_EXIT;}

int epfd;			// MainProcessのepoll fd
epoll_event events[MAX_EPOLL_EVENT_NUM];	// epollのeventを保存します
list<int> cs;		// 登録した顧客のsockｆｄを保存して、Chatの送信と受信で使われます

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

#endif
