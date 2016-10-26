
#include "utility.h"
#include "sqllib.h"

void client_connect(int sockfd);
void *client_response(void *arg);
void response_router(int sockfd, vector<string> vs);
	void Login(int sockfd, vector<string> vs);
	void Register(int sockfd, vector<string> vs);
	void Chat(int sockfd, vector<string> vs);
	void Search(int sockfd, vector<string> vs);
	void Upload(int sockfd, vector<string> vs);
	void Booking(int sockfd, vector<string> vs);
	void SendFile(int sockfd, vector<string> vs);
	void RecvFile(int sockfd, vector<string> vs);
void client_reply(int sockfd, string reply);

typedef struct
{
	pid_t pid;
	int pipefd[2];
}IPC;
IPC ipc[CHILD_PROCESS_NUM];

typedef struct
{
	sockaddr_in addr;
	char buf[BUFSIZ];
}client_data;

int sig_pipefd[2];			// 统一事件源

typedef struct
{
	string cmd;
	int size;	// Protocol: define the number of parameter in each command
	void (*func)(int, vector<string>);
}Service;

Service sv[] = 
{
	{"login", 		3,	Login 		},
	{"register", 	4,	Register 	},
	{"chat",		3,	Chat 		},
	{"search",		4,	Search 		},
	{"upload", 		8,	Upload 		},
	{"booking",		5,	Booking 	},
	{"pullfile",	3,	SendFile	},
	{"pushfile",	3,	RecvFile	},
};

void sig_handler(int msg)
{
	send(sig_pipefd[WRITE], (char *)&msg, 1, 0);
}

void child_run(int id)
{
	epoll_event events[EPOLL_MAX_EVENT];
	int child_epfd = epoll_create(5);
	int pipefd = ipc[id].pipefd[READ];
	epfd_add(child_epfd, pipefd, true);

	//sig_add()

	bool stop_child = false;
	client_data *users = new client_data[USER_MAX_NUM];

	cout<<"child goto work"<<endl;

	while(!stop_child)
	{
		int number = epoll_wait(child_epfd, events, EPOLL_MAX_EVENT, -1);
		
		for(int i = 0; i < number; i++)
		{
			int sockfd = events[i].data.fd;
			if(sockfd == pipefd && events[i].events & EPOLLIN)
			{
				int pick = 1;
				int ret = recv(sockfd, (char *)&pick, sizeof(pick), 0);
				if(0 == ret)
				{
					cout<<"stop child"<<endl;
				//	stop_child = true;	//??
				}
				else
				{
					struct sockaddr_in client_addr;
					socklen_t client_addr_length = sizeof(client_addr);
					int conn = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_length);
					users[conn].addr = client_addr;
					epfd_add(child_epfd, conn, true);
					cout<<"child accept new client"<<endl;
				}
			}
			else if(events[i].events & EPOLLIN)
			{
			//	while(1)
				{
					int ret = recv(sockfd, users[sockfd].buf, BUFSIZ, 0);
					if(0 < ret)
					{
						if(errno == EAGAIN)
						{
							cout<<"read later"<<endl;
							break;
						}
						epfd_del(child_epfd, sockfd);
						close(sockfd);
						break;
					}
					else if(0 == ret)
					{
						epfd_del(child_epfd, sockfd);
						close(sockfd);
						break;
					}
					else
					{
						cout<<"client :"<<users[sockfd].buf<<endl;

					}
				}
			}
			else
				;
		}
	}
}

int main(int ac, char *av[])
{
	int ret = 0;
	int listener = make_server_socket(IP, PORT);
	
	for(int i = 0; i < CHILD_PROCESS_NUM; i++)
	{
		socketpair(AF_UNIX, SOCK_STREAM, 0, ipc[i].pipefd);

		ipc[i].pid = fork();
		if(ipc[i].pid > 0)
		{
			close(ipc[i].pipefd[READ]);
			set_unblocking(ipc[i].pipefd[WRITE]);
			continue;
		}
		else
		{
			close(ipc[i].pipefd[WRITE]);
			set_unblocking(ipc[i].pipefd[READ]);
			child_run(i);
			exit(0);
		}
	}

	Try(epfd = epoll_create(5))
	epoll_event events[EPOLL_MAX_EVENT];
	epfd_add(epfd, listener, true);		// register fd into epoll with ET mode

//	统一事件源,socket和signal都在epolls_wait中处理
	socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
	set_unblocking(sig_pipefd[WRITE]);
	epfd_add(epfd, sig_pipefd[READ], true);

	sig_add(SIGCHLD, sig_handler);
	sig_add(SIGTERM, sig_handler);
	sig_add(SIGPIPE, SIG_IGN);

	bool stop_server = false;
	int child_id = 0;

//	daemon(0,0);

	int event_cnt;
	while(!stop_server)
	{
		Try((event_cnt = epoll_wait(epfd, events, EPOLL_MAX_EVENT, -1)))

		for(int i = 0; i < event_cnt; i++)
		{
			int sockfd = events[i].data.fd;

			if(sockfd == listener)
			{
				int call = 1;
				cout<<"new client"<<endl;
				send(ipc[child_id].pipefd[WRITE], (char *)&call, sizeof(call), 0);
				child_id++;
				child_id %= CHILD_PROCESS_NUM;
			}
			else if(sockfd == sig_pipefd[READ] && events[i].events & EPOLLIN)
			{
				char buf[BUFSIZ]; bzero(buf, BUFSIZ);
				recv(sig_pipefd[READ], buf, BUFSIZ, 0);
				cout<<"main process recv sig"<<endl;
			}
		}
	}

	close(listener);
	close(epfd);
	return 0;
}

void client_connect(int sockfd)
{
	int clientfd;
	sockaddr_in client_address;
	socklen_t addrlen = sizeof(sockaddr_in);

	Try((clientfd = accept(sockfd, (sockaddr *)&client_address, &addrlen)))
	cs.push_back(clientfd);
	epfd_add(epfd, clientfd, true);

	printf("client : %s : % d(IP : port), fd = %d \n",
    		inet_ntoa(client_address.sin_addr),
    		ntohs(client_address.sin_port),
    		clientfd);
}

void *client_response(void *arg)
{
	int sockfd = 0;
	int len;
	char buf[BUFSIZ]; bzero(buf, BUFSIZ);
	
	Try(len = recv(sockfd, buf, BUFSIZ, 0))
	if(0 == len)
	{
		cout<<"******client exit******"<<endl;
		cs.remove(sockfd);
		close(sockfd);
		epfd_del(epfd, sockfd);		// donot monitor sign out usr
		wesql.ClearAddr(sockfd);	// init chat_addr which has been sign out
	}
	else
		response_router(sockfd, split(buf, TOKEN));
}

void response_router(int sockfd, vector<string> vs)
{
	for(int i = 0; i < vs.size(); i++)
		cout<<"param["<<i<<"] = "<<vs[i]<<endl;
	cout<<"---------------------"<<endl;

	for(int i = 0; i < sizeof(sv)/sizeof(sv[0]); i++)
	{	
		if(sv[i].cmd == vs[0])
		{
			if(vs.size() != sv[i].size)			// number of parameter is incorrect
				{client_reply(sockfd, "invalid\n");	return;}
			{sv[i].func(sockfd, vs); return;}	// valid command, action it
		}
	}
	client_reply(sockfd, "undefined\n");		// undefined command
}

void Login(int sockfd, vector<string> vs)
{
	login_info usr;
	usr.name = vs[1];
	usr.pwd = vs[2];
	
	client_reply(sockfd, wesql.Login(usr, sockfd) ? "success\n":"fail\n"); // chat_addr will be set if sign in success
}

void Register(int sockfd, vector<string> vs)
{
	register_info usr;
	usr.name = vs[1];
	usr.pwd = vs[2];
	usr.email = vs[3];
	
	client_reply(sockfd, wesql.Register(usr) ? "success\n":"fail\n");
}

void Chat(int sockfd, vector<string> vs)
{
	string from = wesql.FindNameFromAddr(sockfd);	// get user's chat_addr
	string msg = "<" + from + ">:" + vs[2] + '\n';
	if("all" == vs[1])
	{
		list<int>::iterator it;
    	for(it = cs.begin(); it != cs.end(); it++)
			if(sockfd != *it)
				client_reply(*it, msg.c_str());		// broadcast, donot send to myself
	}
	else
	{
		int to = atoi(wesql.FindAddrFromName(vs[1]).c_str());
		if(-1 == to)
			client_reply(sockfd, "user unlogin\n");
		else
			client_reply(to, msg.c_str());
	}
}

void Search(int sockfd, vector<string> vs)
{
	vector<string> db_res;
	search_info info;
	info.date = vs[1];
	info.start = vs[2];
	info.end = vs[3];

	db_res = wesql.Search(info);
	if(0 == db_res.size())
		{client_reply(sockfd, "noresults\n"); return;}

	string msg;
	vector<string>::iterator it;
	for(it = db_res.begin(); it != db_res.end(); it++)	// bug 由于client的epoll监听是同一事件,连续send两次消息,client也只能处理一次消息
		msg += *it + '|';
	msg += '\n';		// for Android recv, add '\n' at end of string !!!!

	client_reply(sockfd, msg.c_str());
}

void Upload(int sockfd, vector<string> vs)
{
	carpool_info info;
	info.name = vs[1];
	info.date = vs[2];
	info.start = vs[3];
	info.end = vs[4];
	info.price = vs[5];
	info.seat = vs[6];
	info.comment = vs[7];
	
	client_reply(sockfd, wesql.Upload(info) ? "success\n":"fail\n");
}

void Booking(int sockfd, vector<string> vs)
{
	booking_info info;
	info.name = vs[1];
	info.date = vs[2];
	info.start = vs[3];
	info.end = vs[4];

	client_reply(sockfd, wesql.Booking(info) ? "success\n":"fail\n");
}

void SendFile(int sockfd, vector<string> vs)
{
	string filename = FILE_PATH + vs[1] + "." + vs[2];
	int fd = open(filename.c_str(), O_RDONLY);

	struct stat stat_buf;
	fstat(fd, &stat_buf);

	usleep(1);
	Try(sendfile(sockfd, fd, NULL, stat_buf.st_size))	// sendfile: zero copy by kernel
}

void RecvFile(int sockfd, vector<string> vs)
{
	set_blocking(sockfd);

	FILE *f;
	string filename = FILE_PATH + vs[1] + "." + vs[2];
	char buf[BUFSIZ]; bzero(buf, BUFSIZ);

	if(NULL == (f = fopen(filename.c_str(), "wb+")))
		myErr;

	int len = 0;
	while(len = recv(sockfd, buf, BUFSIZ, 0))
	{
		cout<<"recv "<<len<<" bytes"<<endl;
		if(len < 0)	myErr;
		if(len > fwrite(buf, sizeof(char), len, f))
			myErr;
	}
	cout<<"recv file finished"<<endl;
	cout<<"---------------------"<<endl;
	fclose(f);

	set_unblocking(sockfd);
}

void client_reply(int sockfd, string reply)
{
	Try(send(sockfd, reply.c_str(), BUFSIZ, 0))
}