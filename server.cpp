#define DEBUG

#include "utility.h"
#include "sqllib.h"

void init_socket(const char *ip, int port);
void init_child_process();
void init_epoll();
void init_signal();
void sig_handler(int msg);

void polling_allocate_task();

void child_run(int id);
void child_task_over(int sig);
void child_killed(int sig);

void response_router(int sockfd, vector<string> str);
	void req_login(int sockfd, vector<string>& str);
	void req_register(int sockfd, vector<string>& str);
	void req_chat(int sockfd, vector<string>& str);
	void req_search(int sockfd, vector<string>& str);
	void req_upload(int sockfd, vector<string>& str);
	void req_booking(int sockfd, vector<string>& str);
	void req_sendfile(int sockfd, vector<string>& str);
	void req_recvfile(int sockfd, vector<string>& str);
	void req_debug(int sockfd, vector<string>& str);
void response_reply(int sockfd, string reply);

/*
	MainProcessとSubProcess間の通信手段です
	pid：
		Debugのために、各SubProcessのpidを保存します
	pipefd：
		MainProcess側がpipefd[1]に書き込む、SubProcess側がpipefd[0]を監視しているから通知されます
*/
typedef struct
{
	pid_t pid;
	int pipefd[2];
}internal_process_communication;

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

// SubProcessが動くかどうかのFlagです
bool child_stop = false;
// サーバのsocket fdです
int listener;
/* 	MainProcess側がsock監視とSignal監視を統一するために、
	Signalをキャッチすると、直接に処理することではなく、sig_pipefd[1]に書き込む
	MainProcessのWhile(1)循環で、sig_pipefd[0]より信号を取得して処理します
*/
int sig_pipefd[2];			// 统一事件源
// SubProcessのPollです
internal_process_communication childs[MAX_CHILD_PROCESS_NUM];
// ユーザーRequestのRouterみたいなものです
Request request[] = 
{
	{"login", 		3,	req_login 		},
	{"register", 	4,	req_register 	},
	{"chat",		3,	req_chat 		},
	{"search",		4,	req_search 		},
	{"upload", 		8,	req_upload 		},
	{"booking",		5,	req_booking 	},
	{"pullfile",	3,	req_sendfile	},
	{"pushfile",	3,	req_recvfile	},

#ifdef DEBUG
	{"debug",		1,	req_debug		},
#endif
};

/*
	telnetでサーバを訪問して、Debug用の関数です
*/
void req_debug(int sockfd, vector<string>& str)
{
	int msg = 1;
	CHK_ERROR(send(sig_pipefd[WRITE], (char *)&msg, 1, 0));
}

int main(int ac, char *av[])
{
	init_socket(IP, PORT);
	init_child_process();
	init_epoll();
	init_signal();

	bool server_stop = false;
	while(!server_stop)
	{	
		// epfdに登録したｆｄの"読み込める"事件を待っています
		int event_cnt = epoll_wait(epfd, events, MAX_EPOLL_EVENT_NUM, -1);
		if(event_cnt < 0)
		{
			/* 	Signal発生したら、sig_pipefd[0]事件が来ます。
				でもその時KernelがSignalのせいで割り込むがあり、
				epoll_waitが失敗しかねます。
				それはエラーではなく、もう一回epoll_waitすれば大丈夫です
			*/
			if(EINTR == errno)
				continue;
			ERROR_EXIT;
		}
		// 発生した事件を全部取得しました、一個一個対応します
		for(int i = 0; i < event_cnt; i++)
		{
			int sockfd = events[i].data.fd;

			// サーバのsocketが訪問された、それは新しいクライアントが来ました、SubProcessに任せます
			if(sockfd == listener)
			{
				polling_allocate_task();
			}
			// LinuxのSignalが発生しました、何かSignalがあったか、取得して対応します
			else if((sockfd == sig_pipefd[READ]) && (events[i].events & EPOLLIN))
			{
				int ret = 0;
				char signals[BUFSIZ]; bzero(signals, BUFSIZ);
				CHK_ERROR(ret = recv(sig_pipefd[READ], signals, BUFSIZ, 0));
				for(int i = 0; i < ret; i++)
				{
					switch(signals[i])
					{
						// SubProcessが終了しました、MainProcessがお父さんとして回収すべきです
						case SIGCHLD:
							child_task_over(i);
							break;
						// MainProcessがもしCtrl+C、KillなどのSignalをもらったら、SubProcessを終了します
						case SIGTERM:
						case SIGINT:
							for(int i = 0; i < MAX_CHILD_PROCESS_NUM; i++)
							{
								cout<<"kill child "<<childs[i].pid<<endl;
								kill(childs[i].pid, SIGTERM);
								server_stop = true;
								exit(0);
							}
							server_stop = true;
							break;
						default:
							break;
					}
				}
			}
			else
				cout<<"88888888888888888"<<endl;
		}
	}

	close(listener);
	close(epfd);
	return 0;
}

/*
	役割：サーバのsocketを準備します
	
*/
void init_socket(const char *ip, int port)
{
	int yes = 1;
	int server_socket;

	CHK_ERROR(server_socket = socket(AF_INET, SOCK_STREAM, 0));
	// サーバ終了してから、監視Portをすぐ再利用できるようにします
	CHK_ERROR(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)));

	sockaddr_in addr;
	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	CHK_ERROR(bind(server_socket, (sockaddr *)&addr, sizeof(sockaddr_in)));	
	CHK_ERROR(listen(server_socket, 5));

	listener = server_socket;
}

/* 
	Process　Pollを初期化
*/
void init_child_process()
{
	for(int i = 0; i < MAX_CHILD_PROCESS_NUM; i++)
	{
		// Main/Sub　Processが通信できるために、socketpairで設置します
		CHK_ERROR(socketpair(PF_UNIX, SOCK_STREAM, 0, childs[i].pipefd));
		// SubProcessを作成します
		childs[i].pid = fork();
		// ここはMainProcessです
		if(childs[i].pid > 0)
		{
			close(childs[i].pipefd[READ]);
			set_unblocking(childs[i].pipefd[WRITE]);
			continue;
		}
		// ここは作り出したSubProcessです、最後にchild_runに入ります
		else
		{
			childs[i].pid = getpid();
			close(childs[i].pipefd[WRITE]);
			set_unblocking(childs[i].pipefd[READ]);
			child_run(i);
			exit(0);
		}
	}
}

/*
	MainProcessのepollを作成します
	そしてサーバのsocketを登録して、監視し始めます
*/
void init_epoll()
{
	CHK_ERROR(epfd = epoll_create(5));
	epfd_add(epfd, listener);
}

/*
	対応したいLinuxのSignalを登録します
*/
void init_signal()
{
	//	イベント処理の統一化、socketとsignalを全部epolls_waitより監視します
	CHK_ERROR(socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd));
	set_unblocking(sig_pipefd[WRITE]);
	epfd_add(epfd, sig_pipefd[READ]);

	sig_add(SIGCHLD, sig_handler);	// recycle child process
	sig_add(SIGTERM, sig_handler);	// kill pid
	sig_add(SIGINT, sig_handler);	// ctrl + c
	sig_add(SIGPIPE, SIG_IGN);		// ignore SIGPIPE
}

/*
	役割：
		SignalのHandler関数です
	変数：
		msg:キャッチしたSignalの種類
	Process：
		一旦Signalをキャッチすると、sig_pipefd[1]に書き込むより、epoll_waitに通知します
*/
void sig_handler(int msg)
{
	CHK_ERROR(send(sig_pipefd[WRITE], (char *)&msg, 1, 0));
}

/*
	役割：
		ポーリング方式で、
	Process：
		順番に各SubProcessを呼びます
*/
void polling_allocate_task()
{
	static int polling_id = 0;
	int call = 1;
	send(childs[polling_id++].pipefd[WRITE], (char *)&call, sizeof(call), 0);
	
	polling_id %= MAX_CHILD_PROCESS_NUM;
}

/*
	役割：
		SubProcesｓはここに動いてます
	変数：
		id:SubProcessのID
*/
void child_run(int id)
{
	// 各SubProcessが別々に、自分のepoll　eventを作成します
	epoll_event events[MAX_EPOLL_EVENT_NUM];
	int child_epfd = epoll_create(5);
	// 各SubProcessがMainProcessと連絡用のpipefdを監視し始めます
	int pipefd = childs[id].pipefd[READ];
	epfd_add(child_epfd, pipefd);
	// SubProcessがもしkill命令を受信したら、対応します
	sig_add(SIGTERM, child_killed);

	while(!child_stop)
	{
		int number = epoll_wait(child_epfd, events, MAX_EPOLL_EVENT_NUM, -1);
		for(int i = 0; i < number; i++)
		{
			int sockfd = events[i].data.fd;
			// pipefdの事件、MainProcessから呼びかけました、顧客さん来たよ
			if((sockfd == pipefd) && (events[i].events & EPOLLIN))
			{
				struct sockaddr_in addr; 
				bzero((void *)&addr, sizeof(addr));
				socklen_t addr_len = sizeof(sockaddr_in);

				// 顧客さんをacceptします
				int conn = 0;
				CHK_ERROR(conn = accept(listener, (struct sockaddr *)&addr, &addr_len));
				
				// 
				epfd_add(child_epfd, conn);

#ifdef DEBUG
				printf("client : %s:%d fd = %d \n",
			    		inet_ntoa(addr.sin_addr),
			    		ntohs(addr.sin_port),
			    		conn);
#endif

			}
			else if(events[i].events & EPOLLIN)
			{
				char buf[BUFSIZ]; 
				bzero(buf, BUFSIZ);
				int ret = recv(sockfd, buf, BUFSIZ, 0);
				if(0 > ret)
				{
					if(errno == EAGAIN)
						continue;
					child_stop = true;
				}
				else if(0 == ret)
				{
					cs.remove(sockfd);
					wesql.ClearAddr(sockfd);	// init chat_addr which has been sign out
					epfd_del(child_epfd, sockfd);
					close(sockfd);
					break;
				}
				else
				{
					response_router(sockfd, split(buf));
				}
				
			}
			else
				cout<<"99999999999"<<endl;
		}
	}

	close(pipefd);
	close(child_epfd);
}

/*
	SubProcess終了したら、MainProcessが回収します
*/
void child_task_over(int sig)
{
	pid_t pid;
    int stat;
    while ((pid = waitpid( -1, &stat, WNOHANG )))
    {
        continue;
    }
    	
#ifdef DEBUG
    cout<<"child task over!"<<endl;
#endif
}

void child_killed(int sig)
{
	child_stop = true;
}

/*
	受信したユーザーの命令を対応します
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

void req_login(int sockfd, vector<string>& str)
{
	login_info usr;
	usr.name = str[1];
	usr.pwd = str[2];

	if(wesql.Login(usr, sockfd))
	{
		//　顧客さんが正しくログインしたよ、連絡sockｆｄをChatListに追加します
		cs.push_back(sockfd);
		response_reply(sockfd, REPLY_SUCCESS);
	}
	else
		response_reply(sockfd, REPLY_FAILED);
}

void req_register(int sockfd, vector<string>& str)
{
	register_info usr;
	usr.name = str[1];
	usr.pwd = str[2];
	usr.email = str[3];
	
	response_reply(sockfd, wesql.Register(usr) ? REPLY_SUCCESS : REPLY_FAILED);
}

void req_chat(int sockfd, vector<string>& str)
{
	string from = wesql.FindNameFromAddr(sockfd);	// get user's chat_addr
	string msg = "<" + from + ">:" + str[2] + '\n';
	// Chat内容はログインした全員に送ります
	if("all" == str[1])
	{
		list<int>::iterator it;
    	for(it = cs.begin(); it != cs.end(); it++)
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
	相乗りの情報を入力して、検索します
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
	for(it = db_res.begin(); it != db_res.end(); it++)	// bug 由于client的epoll监听是同一事件,连续send两次消息,client也只能处理一次消息
		msg += *it + '|';
	msg += '\n';		// for Android recv, add '\n' at end of string !!!!

	response_reply(sockfd, msg.c_str());
}

/*
	車主は出発情報を登録します
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
	出発の車を予約します
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
	Androidにファイルを送信します（使われないです）
*/
void req_sendfile(int sockfd, vector<string>& str)
{
	string filename = FILE_PATH + str[1] + "." + str[2];
	int fd = open(filename.c_str(), O_RDONLY);

	if(-1 == fd)
		return response_reply(sockfd,REPLY_NORESULTS);

	struct stat stat_buf;
	fstat(fd, &stat_buf);
	usleep(1);
	CHK_ERROR(sendfile(sockfd, fd, NULL, stat_buf.st_size));	// sendfile: zero copy by kernel
}

/*
	Android側ファイルを受信します
*/
void req_recvfile(int sockfd, vector<string>& str)
{
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
