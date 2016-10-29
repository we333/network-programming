#include <list>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "config.h"
#include "worker.h"
#include "utility.h"

using namespace std;

void init_socket(const char *ip, int port);
void init_worker_process();
void init_epoll();
void init_signal();
void sig_handler(int msg);

void polling_allocate_task();

void worker_run(int id);
void worker_task_over(int sig);
void worker_killed(int sig);

int epfd;			// MainProcessのepoll fd
epoll_event events[MAX_EPOLL_EVENT_NUM];	// epollのeventを保存します
list<int> cs;		// 登録した顧客のsockｆｄを保存して、Chatの送信と受信で使われます

typedef struct
{
	pid_t pid;
	int pipefd[2];
}InternalProcessCommunication;

// SubProcessが動くかどうかのFlagです
bool worker_stop = false;
// サーバのsocket fdです
int listener;
/* 	MainProcess側がsock監視とSignal監視を統一するために、
	Signalをキャッチすると、直接に処理することではなく、sig_pipefd[1]に書き込む
	MainProcessのWhile(1)循環で、sig_pipefd[0]より信号を取得して処理します
*/
int sig_pipefd[2];
// SubProcessのPollです
InternalProcessCommunication workers[MAX_CHILD_PROCESS_NUM];
// ユーザーRequestのRouterみたいなものです

/*
	MainProcessとSubProcess間の通信手段です
	pid：
		Debugのために、各SubProcessのpidを保存します
	pipefd：
		MainProcess側がpipefd[1]に書き込む、SubProcess側がpipefd[0]を監視しているから通知されます
*/



int main(int ac, char *av[])
{
	init_socket(av[1], atoi(av[2]));
	init_worker_process();
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

			// 新しいクライアントが来ました、SubProcessに任せます
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
						// MainProcessがもしCtrl+C、KillなどのSignalをもらったら、SubProcessを終了します
						case SIGTERM:
						case SIGINT:
							for(int i = 0; i < MAX_CHILD_PROCESS_NUM; i++)
								kill(workers[i].pid, SIGTERM);
							exit(0);
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
void init_worker_process()
{
	for(int i = 0; i < MAX_CHILD_PROCESS_NUM; i++)
	{
		// Main/Sub　Processが通信できるために、socketpairで設置します
		CHK_ERROR(socketpair(PF_UNIX, SOCK_STREAM, 0, workers[i].pipefd));
		// SubProcessを作成します
		workers[i].pid = fork();
		// ここはMainProcessです
		if(workers[i].pid > 0)
		{
			close(workers[i].pipefd[READ]);
			set_unblocking(workers[i].pipefd[WRITE]);
			continue;
		}
		// ここは作り出したSubProcessです、最後にchild_runに入ります
		else
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
	send(workers[polling_id++].pipefd[WRITE], (char *)&call, sizeof(call), 0);
	
	polling_id %= MAX_CHILD_PROCESS_NUM;
}

/*
	役割：
		SubProcesｓはここに動いてます
	変数：
		id:SubProcessのID
*/
void worker_run(int id)
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
		int number = epoll_wait(worker_epfd, events, MAX_EPOLL_EVENT_NUM, -1);
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

				epfd_add(worker_epfd, conn);

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
					//104 connection reset by peer
					if(errno == EAGAIN || errno == 104)
						continue;
					worker_stop = true;
				}
				else if(0 == ret)
				{
					cs.remove(sockfd);
					epfd_del(worker_epfd, sockfd);
					close(sockfd);
					break;
				}
				else
				{
					response_router(sockfd, split(buf));
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
	SubProcess終了したら、MainProcessが回収します
*/
void worker_task_over(int sig)
{
	pid_t pid;
    int stat;
    while ((pid = waitpid( -1, &stat, WNOHANG )))
    {
        continue;
    }
    	
#ifdef DEBUG
    cout<<"worker task over!"<<endl;
#endif
}

void worker_killed(int sig)
{
	worker_stop = true;
}
