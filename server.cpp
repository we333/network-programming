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
void child_task_over(int id);
void client_connect(int sockfd);
void response_router(int sockfd, vector<string> str);
	void req_login(int sockfd, vector<string>& str);
	void req_register(int sockfd, vector<string>& str);
	void req_chat(int sockfd, vector<string>& str);
	void req_search(int sockfd, vector<string>& str);
	void req_upload(int sockfd, vector<string>& str);
	void req_booking(int sockfd, vector<string>& str);
	void req_sendFile(int sockfd, vector<string>& str);
	void req_recvFile(int sockfd, vector<string>& str);
void client_reply(int sockfd, string reply);


typedef struct
{
	pid_t pid;
	int pipefd[2];
}internal_process_communication;

typedef struct
{
	sockaddr_in addr;
	char buf[BUFSIZ];
}client_data;

typedef struct
{
	string cmd;
	int size;
	void (*func)(int, vector<string>&);
}Request;

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
	{"pullfile",	3,	req_sendFile	},
	{"pushfile",	3,	req_recvFile	},
};

int main(int ac, char *av[])
{
	init_socket(IP, PORT);
	init_child_process();
	init_epoll();
	init_signal();
	
	bool stop_server = false;
	while(!stop_server)
	{
		int event_cnt;
		CHK_ERROR(event_cnt = epoll_wait(epfd, events, MAX_EPOLL_EVENT_NUM, -1));

		for(int i = 0; i < event_cnt; i++)
		{
			int sockfd = events[i].data.fd;

			if(sockfd == listener)
			{
				polling_allocate_task();
			}
			else if((sockfd == sig_pipefd[READ]) && (events[i].events & EPOLLIN))
			{
cout<<"signal coming"<<endl;
				int ret = 0;
				char signals[BUFSIZ]; bzero(signals, BUFSIZ);
				CHK_ERROR(ret = recv(sig_pipefd[READ], signals, BUFSIZ, 0));
				for(int i = 0; i < ret; i++)
				{
					switch(signals[i])
					{
						case SIGCHLD:
							
							break;
						case SIGTERM:
						case SIGINT:
							for(int i = 0; i < MAX_CHILD_PROCESS_NUM; i++)
							{
								cout<<"kill child "<<childs[i].pid<<endl;
								kill(childs[i].pid, SIGTERM);
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

	sig_add(SIGCHLD, sig_handler);
	sig_add(SIGTERM, sig_handler);
	sig_add(SIGINT, sig_handler);
	sig_add(SIGPIPE, SIG_IGN);
}

void sig_handler(int msg)
{
	CHK_ERROR(send(sig_pipefd[WRITE], (char *)&msg, 1, 0));
}

void polling_allocate_task()
{
	static int polling_id = 0;
	int call = 1;
	CHK_ERROR(send(childs[polling_id].pipefd[WRITE], (char *)&call, sizeof(call), 0));
	polling_id++;
	polling_id %= MAX_CHILD_PROCESS_NUM;
}

void child_run(int id)
{
	epoll_event events[MAX_EPOLL_EVENT_NUM];
	int child_epfd = epoll_create(5);
	int pipefd = childs[id].pipefd[READ];
	epfd_add(child_epfd, pipefd);

	//sig_add()

	bool stop_child = false;
	client_data *users = new client_data[USER_MAX_NUM];

	while(!stop_child)
	{
		cout<<"pid = "<<childs[id].pid<<endl;
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

				cs.push_back(conn);
				users[conn].addr = addr;
				epfd_add(child_epfd, conn);
			}
			else if(events[i].events & EPOLLIN)
			{
				int ret;
				CHK_ERROR(ret = recv(sockfd, users[sockfd].buf, BUFSIZ, 0));
				if(0 > ret)
				{
					if(errno == EAGAIN)
					{
						cout<<"read later"<<endl;
						break;
					}
					epfd_del(child_epfd, sockfd);
					close(sockfd);
					perror("recv failed: ");
					break;
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
					response_router(sockfd, split(users[sockfd].buf));
				}
				
			}
			else
				cout<<"99999999999"<<endl;
		}
	}
}

void child_task_over(int id)
{
	int status;
    waitpid(-1, &status, WNOHANG);   
    if(!WIFEXITED(status))
    	log_error;
#ifdef DEBUG
    cout<<"child task over!"<<endl;
#endif
}

void client_connect(int sockfd)
{
	int clientfd;
	sockaddr_in client_address;
	socklen_t addrlen = sizeof(sockaddr_in);

	CHK_ERROR(clientfd = accept(sockfd, (sockaddr *)&client_address, &addrlen));
	cs.push_back(clientfd);
	epfd_add(epfd, clientfd);

#ifdef DEBUG
	printf("client : %s : % d(IP : port), fd = %d \n",
    		inet_ntoa(client_address.sin_addr),
    		ntohs(client_address.sin_port),
    		clientfd);
#endif
}

void response_router(int sockfd, vector<string> str)
{
#ifdef DEBUG
	for(int i = 0; i < str.size(); i++)
		cout<<"param["<<i<<"] = "<<str[i]<<endl;
	cout<<"---------------------"<<endl;
#endif

	for(int i = 0; i < sizeof(request)/sizeof(request[0]); i++)
	{	
		if(request[i].cmd == str[0])
		{
			if(str.size() != request[i].size)		// number of parameter is incorrect
				return client_reply(sockfd, REPLY_INVALID);
			return request[i].func(sockfd, str);		// valid command, action it
		}
	}
	client_reply(sockfd, REPLY_UNDEFINED);			// undefined command
}

void req_login(int sockfd, vector<string>& str)
{
	login_info usr;
	usr.name = str[1];
	usr.pwd = str[2];

	// chat_addr will be set if sign in success
	client_reply(sockfd, wesql.Login(usr, sockfd) ? REPLY_SUCCESS : REPLY_FAILED);
}

void req_register(int sockfd, vector<string>& str)
{
	register_info usr;
	usr.name = str[1];
	usr.pwd = str[2];
	usr.email = str[3];
	
	client_reply(sockfd, wesql.Register(usr) ? REPLY_SUCCESS : REPLY_FAILED);
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
				client_reply(*it, msg.c_str());		// broadcast, donot send to myself
	}
	else
	{
		int to = atoi(wesql.FindAddrFromName(str[1]).c_str());
		if(-1 == to)
			client_reply(sockfd, "user unlogin\n");
		else
			client_reply(to, msg.c_str());
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
		return client_reply(sockfd, REPLY_NORESULTS);

	string msg;
	vector<string>::iterator it;
	for(it = db_res.begin(); it != db_res.end(); it++)	// bug 由于client的epoll监听是同一事件,连续send两次消息,client也只能处理一次消息
		msg += *it + '|';
	msg += '\n';		// for Android recv, add '\n' at end of string !!!!

	client_reply(sockfd, msg.c_str());
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
	
	client_reply(sockfd, wesql.Upload(info) ? REPLY_SUCCESS : REPLY_FAILED);
}

void req_booking(int sockfd, vector<string>& str)
{
	booking_info info;
	info.name = str[1];
	info.date = str[2];
	info.start = str[3];
	info.end = str[4];

	client_reply(sockfd, wesql.Booking(info) ? REPLY_SUCCESS : REPLY_FAILED);
}

void req_sendFile(int sockfd, vector<string>& str)
{
	string filename = FILE_PATH + str[1] + "." + str[2];
	int fd = open(filename.c_str(), O_RDONLY);

	struct stat stat_buf;
	fstat(fd, &stat_buf);

	usleep(1);
	CHK_ERROR(sendfile(sockfd, fd, NULL, stat_buf.st_size));	// sendfile: zero copy by kernel
}

void req_recvFile(int sockfd, vector<string>& str)
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
			log_error;
	}

	fclose(f);

	set_unblocking(sockfd);
}

void client_reply(int sockfd, string reply)
{
	CHK_ERROR(send(sockfd, reply.c_str(), BUFSIZ, 0));
}
