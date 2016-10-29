/**************************************************************************

Copyright:duyan

Author: duyan

Date:2016-10-28

Description: It's a Half-Sync/Half-Async Architectural TCP server

**************************************************************************/

#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <list>

#include "config.h"
#include "worker.h"
#include "utility.h"

using namespace std;

static void init_socket(const char *ip, int port);
static void init_worker_process(int num);
static void init_epoll();
static void init_signal();
static void sig_handler(int msg);

static void polling_allocate_task();

static void worker_run(int id);
static void worker_task_over(int sig);
static void worker_killed(int sig);

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
}InternalProcessCommunication;

bool worker_stop = false;				// SubProcessが動くかどうかのFlagです
int listener;							// サーバのsocket file descriptorです
int epfd;								// MainProcessのepoll fd
epoll_event events[MAX_EPOLL_EVENT_NUM];// epollのeventを保存します	
list<int> chat;							// 登録した顧客のsockｆｄを保存して、Chatの送信と受信で使われます

/* 	
	MainProcess側がsock監視とSignal監視を統一するために、
	Signalをキャッチすると、直接に処理することではなく、sig_pipefd[1]に書き込む
	MainProcessのWhile(1)循環で、sig_pipefd[0]より信号を取得して処理します
*/
int sig_pipefd[2];
InternalProcessCommunication workers[MAX_WORKER_PROCESS_NUM];	// SubProcessのPollです

/*
Summary: initialize and launch server
Parameters:
    ac : the number of parameters    
    av : av[1] should be a ip address, av[2] should be a listening port 
Return : 0
*/
int main(int ac, char *av[])
{
	init_socket(av[1], atoi(av[2]));
	init_worker_process(MAX_WORKER_PROCESS_NUM);
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

			// 新しいClientが来ました、SubProcessに任せます
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
							worker_task_over(i);
							break;
						// MainProcessがもしCtrl+C、KillなどのSignalをもらったら、SubProcessを終了してから、自分も終了します
						case SIGTERM:
						case SIGINT:
							for(int i = 0; i < MAX_WORKER_PROCESS_NUM; i++)
								kill(workers[i].pid, SIGTERM);
							server_stop = true;
							break;
						default:
							break;
					}
				}
			}
			else
				;
		}
	}

	close(listener);
	close(epfd);
	exit(0);
	return 0;
}

/*
Summary: initialize the socket of server
Parameters:
    ip : server ip
    port : listening port
Return : 
*/
static void init_socket(const char *ip, int port)
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
Summary: initialize servel sub process and ready to work
Parameters:
	num : the number of sub process
Return : 
*/
static void init_worker_process(int num)
{
	for(int i = 0; i < num; i++)
	{
		// Main/Sub　Processが通信できるために、socketpairで設置します
		CHK_ERROR(socketpair(PF_UNIX, SOCK_STREAM, 0, workers[i].pipefd));
		// SubProcessを作成します
		workers[i].pid = fork();
		if(workers[i].pid > 0)	// ここはMainProcessです
		{
			close(workers[i].pipefd[READ]);
			set_unblocking(workers[i].pipefd[WRITE]);
			continue;
		}
		else					// ここは作り出したSubProcessです、最後にchild_runに入ります
		{
			workers[i].pid = getpid();
			close(workers[i].pipefd[WRITE]);
			set_unblocking(workers[i].pipefd[READ]);
			worker_run(i);
			exit(0);
		}
	}
}

/*
	MainProcessのepollを作成します
	そしてサーバのsocketを登録して、監視し始めます
*/
/*
Summary: initialize the epoll I/O
Parameters:
Return :
*/
static void init_epoll()
{
	CHK_ERROR(epfd = epoll_create(5));
	epfd_add(epfd, listener);
}

/*
Summary: set up signal handler
Parameters:
Return : 
*/
static void init_signal()
{
	//	イベント処理の統一化、signalあったら、epolls_waitで対応します
	CHK_ERROR(socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd));
	set_unblocking(sig_pipefd[WRITE]);
	epfd_add(epfd, sig_pipefd[READ]);

	sig_add(SIGCHLD, sig_handler);	// recycle child process
	sig_add(SIGTERM, sig_handler);	// kill pid
	sig_add(SIGINT, sig_handler);	// ctrl + c
	sig_add(SIGPIPE, SIG_IGN);		// ignore SIGPIPE
}

/*
Summary: process received signal
Parameters:
	msg : received signal
Return : 
*/
static void sig_handler(int msg)
{
	// 一旦Signalをキャッチすると、sig_pipefd[1]に書き込むより、epfdに通知します
	CHK_ERROR(send(sig_pipefd[WRITE], (char *)&msg, 1, 0));
}

/*
Summary: awaken sub process to work
Parameters:
Return : 
*/
static void polling_allocate_task()
{
	static int polling_id = 0;
	int call = 1;
	// ポーリング方式で、順番に各SubProcessを呼びます
	send(workers[polling_id++].pipefd[WRITE], (char *)&call, sizeof(call), 0);
	polling_id %= MAX_WORKER_PROCESS_NUM;
}

/*
Summary: sub process's workspace
Parameters:
	id : the ID of process
Return : 
*/
static void worker_run(int id)
{
	// 各SubProcessが別々に、自分のepoll　eventを作成します
	epoll_event events[MAX_EPOLL_EVENT_NUM];
	int worker_epfd = epoll_create(5);
	// 各SubProcessがMainProcessと連絡用のpipefdを監視し始めます
	int pipefd = workers[id].pipefd[READ];
	epfd_add(worker_epfd, pipefd);
	// SubProcessがもしkill命令を受信したら、対応します
	sig_add(SIGTERM, worker_killed);

	while(!worker_stop)
	{
		int event_cnt = epoll_wait(worker_epfd, events, MAX_EPOLL_EVENT_NUM, -1);
		for(int i = 0; i < event_cnt; i++)
		{
			int sockfd = events[i].data.fd;
			// pipefdの事件、MainProcessから呼びかけました、Client来たよ
			if((sockfd == pipefd) && (events[i].events & EPOLLIN))
			{
				struct sockaddr_in addr; 
				bzero((void *)&addr, sizeof(addr));
				socklen_t addr_len = sizeof(sockaddr_in);

				// Clientをacceptします
				int conn = 0;
				CHK_ERROR(conn = accept(listener, (struct sockaddr *)&addr, &addr_len));

				epfd_add(worker_epfd, conn);

#ifdef DEBUG
				printf("client : %s:%d fd = %d \n",
			    		inet_ntoa(addr.sin_addr),
			    		ntohs(addr.sin_port),
			    		conn);
#endif

			}
			// Clientからメッセージを受信します
			else if(events[i].events & EPOLLIN)
			{
				char buf[BUFSIZ]; 
				bzero(buf, BUFSIZ);
				int ret = recv(sockfd, buf, BUFSIZ, 0);
				if(0 > ret)
				{
					//104 connection reset by peer
					if(errno != EAGAIN && errno != 104)
						worker_stop = true;
				}
				else if(0 == ret)
				{
					// delete client's socket from chat list
					chat.remove(sockfd);
					// epoll do not monitor client's socket
					epfd_del(worker_epfd, sockfd);
					close(sockfd);
					break;
				}
				else
				{
					// Clientの要求を処理します
					vector<string> msg = split(buf);
					response_router(sockfd, msg);
				}
				
			}
			else
				;
		}
	}

	close(pipefd);
	close(worker_epfd);
	exit(0);
}

/*
Summary : main process recycle sub process
Parameters:
	sig : unused
Return : 
*/
static void worker_task_over(int sig)
{
	pid_t pid;
    int stat;
    while ((pid = waitpid( -1, &stat, WNOHANG )))
        continue;
    	
#ifdef DEBUG
    cout<<"worker task over!"<<endl;
#endif
}

/*
Summary : sub process received kill command
Parameters:
	sig : unused
Return : 
*/
static void worker_killed(int sig)
{
	worker_stop = true;
}
