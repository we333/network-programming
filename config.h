#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#define DEBUG

#define MAX_WORKER_PROCESS_NUM	(5)		// SubProcessの数
#define READ 					(0)		// Pipeを読み込み
#define WRITE					(1)		//　Pipeを書き込み
#define MAX_EPOLL_EVENT_NUM 	(4096)	// epoll監視イベントの数

#define TOKEN 			("|")			// Clientからもらった"param1|param2|param3"のメッセージを分解
#define FILE_PATH 		("static/")		// ファイルを保存する場所

// ユーザーへ返事の内容
#define REPLY_SUCCESS	("success\n")
#define REPLY_NORESULTS ("noresults\n")
#define REPLY_FAILED	("fail\n")
#define REPLY_INVALID	("invalid\n")
#define REPLY_UNDEFINED	("undefined\n")
#define REPLY_UNLOGINED	("user unlogin\n")

// コードを簡潔するために、エラーチェックをマクロにします
#define	ERROR_EXIT		{cout<<__FUNCTION__<<": "<<__LINE__<<" line"<<endl; perror(""); exit(-1);}
#define CHK_ERROR(x)	{if(0 > (x))		ERROR_EXIT;}
#define CHK_NULL(x)		{if(NULL == (x))	ERROR_EXIT;}

#endif