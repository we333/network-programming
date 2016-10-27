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


typedef struct
{
	pid_t pid;
	int pipefd[2];
}internal_process_communication;

typedef struct
{
	string cmd;
	int size;
	void (*func)(int, vector<string>&);
}Request;

bool child_stop = false;
int listener;
int sig_pipefd[2];			// 统一事件源
internal_process_communication childs[MAX_CHILD_PROCESS_NUM];

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
		int event_cnt = epoll_wait(epfd, events, MAX_EPOLL_EVENT_NUM, -1);
		if(event_cnt < 0)
		{
			if(EINTR == errno)
				continue;
			ERROR_EXIT;
		}

		for(int i = 0; i < event_cnt; i++)
		{
			int sockfd = events[i].data.fd;

			if(sockfd == listener)
			{
				polling_allocate_task();
			}
			else if((sockfd == sig_pipefd[READ]) && (events[i].events & EPOLLIN))
			{
				int ret = 0;
				char signals[BUFSIZ]; bzero(signals, BUFSIZ);
				CHK_ERROR(ret = recv(sig_pipefd[READ], signals, BUFSIZ, 0));
				for(int i = 0; i < ret; i++)
				{
					switch(signals[i])
					{
						case SIGCHLD:
							child_task_over(i);
							break;
						case SIGTERM:
						case SIGINT:
							for(int i = 0; i < MAX_CHILD_PROCESS_NUM; i++)
							{
								cout<<"kill child "<<childs[i].pid<<endl;
								kill(childs[i].pid, SIGTERM);
								server_stop = true;
								exit(0);
							}
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

void init_socket(const char *ip, int port)
{
	int yes = 1;
	int server_socket;

	CHK_ERROR(server_socket = socket(AF_INET, SOCK_STREAM, 0));
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

void init_child_process()
{
	for(int i = 0; i < MAX_CHILD_PROCESS_NUM; i++)
	{
		CHK_ERROR(socketpair(PF_UNIX, SOCK_STREAM, 0, childs[i].pipefd));
	
		childs[i].pid = fork();
		if(childs[i].pid > 0)
		{
			close(childs[i].pipefd[READ]);
			set_unblocking(childs[i].pipefd[WRITE]);
			continue;
		}
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

void init_epoll()
{
	CHK_ERROR(epfd = epoll_create(5));
	epfd_add(epfd, listener);
}

void init_signal()
{
	//	统一事件源,socket和signal都在epolls_wait中处理
	CHK_ERROR(socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd));
	set_unblocking(sig_pipefd[WRITE]);
	epfd_add(epfd, sig_pipefd[READ]);

	sig_add(SIGCHLD, sig_handler);	// recycle child process
	sig_add(SIGTERM, sig_handler);	// kill pid
	sig_add(SIGINT, sig_handler);	// ctrl + c
	sig_add(SIGPIPE, SIG_IGN);		// ignore SIGPIPE
}

void sig_handler(int msg)
{
	CHK_ERROR(send(sig_pipefd[WRITE], (char *)&msg, 1, 0));
}

void polling_allocate_task()
{
	static int polling_id = 0;
	int call = 1;
	send(childs[polling_id++].pipefd[WRITE], (char *)&call, sizeof(call), 0);
	
	polling_id %= MAX_CHILD_PROCESS_NUM;
}

void child_run(int id)
{
	epoll_event events[MAX_EPOLL_EVENT_NUM];
	int child_epfd = epoll_create(5);
	int pipefd = childs[id].pipefd[READ];
	epfd_add(child_epfd, pipefd);

	sig_add(SIGTERM, child_killed);

	while(!child_stop)
	{
		int number = epoll_wait(child_epfd, events, MAX_EPOLL_EVENT_NUM, -1);
		for(int i = 0; i < number; i++)
		{
			int sockfd = events[i].data.fd;
			if((sockfd == pipefd) && (events[i].events & EPOLLIN))
			{
				struct sockaddr_in addr; 
				bzero((void *)&addr, sizeof(addr));
				socklen_t addr_len = sizeof(sockaddr_in);

				int conn = 0;
				CHK_ERROR(conn = accept(listener, (struct sockaddr *)&addr, &addr_len));
				cout<<"accept res = "<<conn<<endl;

				cs.push_back(conn);
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

	// chat_addr will be set if sign in success
	response_reply(sockfd, wesql.Login(usr, sockfd) ? REPLY_SUCCESS : REPLY_FAILED);
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
	if("all" == str[1])
	{
		list<int>::iterator it;
    	for(it = cs.begin(); it != cs.end(); it++)
			if(sockfd != *it)
				response_reply(*it, msg.c_str());		// broadcast, donot send to myself
	}
	else
	{
		int to = atoi(wesql.FindAddrFromName(str[1]).c_str());
		if(-1 == to)
		{
		cout<<"11111"<<endl;
			response_reply(sockfd, "user unlogin\n");
		}
		else
		{
		cout<<"22222 to = "<<to<<endl;
			response_reply(to, msg.c_str());
		}
	}
}

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

void req_booking(int sockfd, vector<string>& str)
{
	booking_info info;
	info.name = str[1];
	info.date = str[2];
	info.start = str[3];
	info.end = str[4];

	response_reply(sockfd, wesql.Booking(info) ? REPLY_SUCCESS : REPLY_FAILED);
}

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
