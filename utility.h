//#ifndef UTILITY_H_INCLUDED
//#define UTILITY_H_INCLUDED

#include <iostream>
#include <vector>



#include "config.h"

using namespace std;


/*
	ユーザーの命令を分析します

	chat|we|hello -> weに"hello"を送信します
*/
vector<string> split(char *buf);

/*
	epollのEdge　Triggerモードに、sockｆｄは非閉塞
*/
void set_unblocking(int fd);

/*
	ファイルを受信するとき、sockｆｄを閉塞にします
*/
void set_blocking(int fd);

/*
	ｆｄをepollfdに追加して、監視し始めます
*/
void epfd_add(int epollfd, int fd);

/*
	fdをepollfdから削除して、監視しない
*/
void epfd_del(int epollfd, int fd);

/*
	SignalのHandler関数を指定します
*/
void sig_add(int sig, void (* handler)(int), bool restart = true);

//#endif
