
#include <sys/types.h>    
#include <sys/stat.h>    
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <stdio.h>
#include <string.h>

#include "worker.h"
#include "config.h"
#include "sqllib.h"
#include "utility.h"

using namespace std;

// chat機能、各ユーザーの送信宛名を保存します
extern list<int> chat;

// ユーザーRequestのRouterです。命令をチェックするために、各命令のサイズを事前に定義します
Request request[] = 
{
	{"login", 		3,	req_login 		},
	{"register", 	4,	req_register 	},
	{"chat",		3,	req_chat 		},
	{"search",		4,	req_search 		},
	{"upload", 		8,	req_upload 		},
	{"booking",		5,	req_booking 	},
	{"checkbooking",2,	req_checkbooking},
	{"pullfile",	3,	req_sendfile	},
	{"pushfile",	3,	req_recvfile	},

#ifdef DEBUG
	{"debug",		1,	req_debug		},
#endif
};

/*
Summary : detect request, and then process it
Parameters:
	sockfd : client' socket descriptor
	str : client's command
Return : 
*/
void response_router(int sockfd, vector<string> str)
{
#ifdef DEBUG
	for(int i = 0; i < str.size(); i++)
		printf("param[%d] = %s\n", i, str[i].c_str());
	cout<<"---------------------"<<endl;
#endif

	for(int i = 0; i < sizeof(request)/sizeof(request[0]); i++)
	{	
		if(request[i].cmd == str[0])
		{
			if(str.size() != request[i].size)			// number of parameter is incorrect
				return response_reply(sockfd, REPLY_INVALID);
			return request[i].func(sockfd, str);		// valid command, action it
		}
	}
	response_reply(sockfd, REPLY_UNDEFINED);			// undefined command
}

/*
Summary : user login
Parameters:
	sockfd : client' socket descriptor
	str : login infomation
Return : 
*/
void req_login(int sockfd, vector<string>& str)
{
	login_info usr;
	usr.name = str[1];
	usr.pwd = str[2];

	if(wesql.Login(usr, sockfd))
	{
		//　顧客さんが正しくログインしたよ、連絡sockｆｄをChatListに追加します
		chat.push_back(sockfd);
		response_reply(sockfd, REPLY_SUCCESS);
	}
	else
		response_reply(sockfd, REPLY_FAILED);
}

/*
Summary : user register
Parameters:
	sockfd : client' socket descriptor
	str : client's request
Return : 
*/
void req_register(int sockfd, vector<string>& str)
{
	register_info usr;
	usr.name = str[1];
	usr.pwd = str[2];
	usr.email = str[3];
	
	response_reply(sockfd, wesql.Register(usr) ? REPLY_SUCCESS : REPLY_FAILED);
}

/*
Summary : user chat (AF_INET is unusabale in multiprocess, except use AF_UNIX setting by socketpair())
Parameters:
	sockfd : client' socket descriptor
	str : chat message
Return : 
*/
void req_chat(int sockfd, vector<string>& str)
{
	string from = wesql.FindNameFromAddr(sockfd);	// get user's chat_addr
	string msg = "<" + from + ">:" + str[2] + '\n';
	// Chat内容はログインした全員に送ります
	if("all" == str[1])
	{
		list<int>::iterator it;
    	for(it = chat.begin(); it != chat.end(); it++)
			if(sockfd != *it)
				response_reply(*it, msg.c_str());		// broadcast, donot send to myself
	}
	// Chat内容は指定の人に送ります
	else
	{
		// 受信側今ログインしているかどうかを確認します
		int to = atoi(wesql.FindAddrFromName(str[1]).c_str());
		if(0 >= to)
		{
			response_reply(sockfd, REPLY_UNLOGINED);
		}
		else
		{
			response_reply(to, msg.c_str());
		}
	}
}
                                                                                                          
/*
Summary : search carpool infomation
Parameters:
	sockfd : client' socket descriptor
	str : search keyword, contains date, from, to
Return : 
*/
void req_search(int sockfd, vector<string>& str)
{
	vector<string> db_res;
	search_info info;
	info.date = str[1];
	info.start = str[2];
	info.end = str[3];

	db_res = wesql.Search(info);
	if(0 == db_res.size())
		return response_reply(sockfd, REPLY_NORESULTS);

	string msg;
	vector<string>::iterator it;
	for(it = db_res.begin(); it != db_res.end(); it++)
		msg += *it + '|';
	msg += '\n';		// for Android recv, add '\n' at end of string !!!!

	response_reply(sockfd, msg.c_str());
}

/*
Summary : upload carpool infomation
Parameters:
	sockfd : client' socket descriptor
	str : carpool information, contains driver name, date, from, to, price, seat number, comment
Return : 
*/
void req_upload(int sockfd, vector<string>& str)
{
	carpool_info info;
	info.name = str[1];
	info.date = str[2];
	info.start = str[3];
	info.end = str[4];
	info.price = str[5];
	info.seat = str[6];
	info.comment = str[7];
	
	response_reply(sockfd, wesql.Upload(info) ? REPLY_SUCCESS : REPLY_FAILED);
}

/*
Summary : booking carpool
Parameters:
	sockfd : client' socket descriptor
	str : booking carpool keyword
Return : 
*/
void req_booking(int sockfd, vector<string>& str)
{
	booking_info info;
	info.name = str[1];
	info.date = str[2];
	info.start = str[3];
	info.end = str[4];

	response_reply(sockfd, wesql.Booking(info) ? REPLY_SUCCESS : REPLY_FAILED);
}

/*
Summary : before booking carpool, check seat is more than one or not
Parameters:
	sockfd : client' socket descriptor
	str : check remain seat by driver name
Return : 
*/
void req_checkbooking(int sockfd, vector<string>& str)
{
	vector<string> db_res = wesql.Check_booking(str[1]);
	
/*	donot check message size, because even receive NULL, it's size not equals 0
	if(0 == db_res.size())
		{client_reply(sockfd, "noresults\n"); return;}
*/
	string msg;
	vector<string>::iterator it;
	for(it = db_res.begin(); it != db_res.end(); it++)
		msg += *it + '|';
	msg += '\n';		// for Android recv(), should add '\n' at the end of string !!!!

	// 捜索した相乗り情報を見つけません
	if("||||||\n" == msg)
		return response_reply(sockfd, REPLY_NORESULTS);

	response_reply(sockfd, msg.c_str());
}

/*
Summary : send file to client by sendfile (zero copy by kernel)
Parameters:
	sockfd : client' socket descriptor
	str : requested file name
Return : 
*/
void req_sendfile(int sockfd, vector<string>& str)
{
	string filename = FILE_PATH + str[1] + "." + str[2];
	int fd = open(filename.c_str(), O_RDONLY);

	if(-1 == fd)
		return response_reply(sockfd,REPLY_NORESULTS);

	struct stat stat_buf;
	fstat(fd, &stat_buf);
	usleep(1);	// must wait a moment before sendfile() !!
	CHK_ERROR(sendfile(sockfd, fd, NULL, stat_buf.st_size));
}

/*
Summary : receive file from client
Parameters:
	sockfd : client' socket descriptor
	str : client's file name
Return : 
*/
void req_recvfile(int sockfd, vector<string>& str)
{
	// receive big data should use blocking mode, it's not a good method
	set_blocking(sockfd);

	FILE *f;
	string filename = FILE_PATH + str[1] + "." + str[2];
	CHK_NULL(f = fopen(filename.c_str(), "wb+"));

	int len = 0;
	char buf[BUFSIZ]; bzero(buf, BUFSIZ);
	while(len = recv(sockfd, buf, BUFSIZ, 0))
	{
		CHK_ERROR(len);
		if(len > fwrite(buf, sizeof(char), len, f))
			ERROR_EXIT;
	}

	fclose(f);
	set_unblocking(sockfd);
}

void response_reply(int sockfd, string reply)
{
	CHK_ERROR(send(sockfd, reply.c_str(), BUFSIZ, 0));
}

void req_debug(int sockfd, vector<string>& str)
{
	
}

