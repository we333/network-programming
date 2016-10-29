#ifndef WORKER_H_INCLUDED
#define WORKER_H_INCLUDED

#include <string>
#include <vector>

using namespace std;

/*
	cmd:
		定義のサーバとユーザーの通信命令集です
	size:
		命令の変数を定義して、通信中正しい命令かどうかをチェックできます
	func:
		命令に対応する処理関数です
*/
typedef struct
{
	string cmd;
	int size;
	void (*func)(int, vector<string>&);
}Request;

void response_router(int sockfd, vector<string>& str);

void req_login(int sockfd, vector<string>& str);
void req_register(int sockfd, vector<string>& str);
void req_chat(int sockfd, vector<string>& str);
void req_search(int sockfd, vector<string>& str);
void req_upload(int sockfd, vector<string>& str);
void req_booking(int sockfd, vector<string>& str);
void req_checkbooking(int sockfd, vector<string>& str);
void req_sendfile(int sockfd, vector<string>& str);
void req_recvfile(int sockfd, vector<string>& str);
void req_debug(int sockfd, vector<string>& str);

void response_reply(int sockfd, string reply);


#endif