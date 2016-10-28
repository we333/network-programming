/*
	1.connect指定的ip和port,获得sock
	2.声明pipe
	3.epoll-->添加监听sock和pipe[0] (Read)
	4.fork()
		1.child-->用fgets从stdin获得输入信息,然后write到pipe[1]
		2.等待epoll返回监听事件数量(非阻塞)
		|
		|--stdin-->自己stdin的输入信息提交给服务器
		|
		|--客户fd-->recv
					|
					|--> =0,服务器断开连接-->关闭fd(客户不关心客户数组&epoll_event)
					|
					|--> >0,接收其他客户的消息并处理(fork)
*/

#include "utility.h"

int init_client_socket(const char *ip, int port)
{
	int client_socket;

	if(-1 == (client_socket = socket(AF_INET, SOCK_STREAM, 0)))
		ERROR_EXIT("client socket failed");

	sockaddr_in addr;
	bzero((void *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if(-1 == connect(client_socket, (sockaddr *)&addr, sizeof(sockaddr_in)))
		ERROR_EXIT("connect failed");

	return client_socket;
}

int main(int ac, char *av[])
{
	bool living = true;
	pid_t pid;
	int sockfd = init_client_socket(IP, PORT);
	int epfd, event_cnt, pipe_fd[2];
	char msg[BUFSIZ]; bzero(msg, BUFSIZ);

	CHK_ERROR(pipe(pipe_fd));

	CHK_ERROR(epfd = epoll_create(MAX_EPOLL_EVENT_NUM));

	struct epoll_event events[2];
	epfd_add(epfd, sockfd);
	epfd_add(epfd, pipe_fd[0]);

	CHK_ERROR(pid = fork());
	if(0 == pid)	// 子进程 write 1
	{
		close(pipe_fd[0]);
		while(living)
		{
			bzero(msg, BUFSIZ);
			fgets(msg, BUFSIZ, stdin);
			if(0 == strncasecmp(msg, "exit", strlen("exit")))
				living = false;
			else
				CHK_ERROR(write(pipe_fd[1], msg, strlen(msg)-1));
		}
		close(pipe_fd[1]);
	}
	else 				// 父进程 read 0
	{
		close(pipe_fd[1]);
		while(living)
		{
			CHK_ERROR(event_cnt = epoll_wait(epfd, events, MAX_EPOLL_EVENT_NUM, -1));
			for(int i = 0; i < event_cnt; i++)
			{
				int ret;
				bzero(msg, BUFSIZ);
				if(sockfd == events[i].data.fd)
				{
					CHK_ERROR(recv(sockfd, msg, BUFSIZ, 0));
					if(0 == ret)	living = false;
					else 			cout<<msg<<endl;	
				}
				else 	// read msg from stdin
				{
					CHK_ERROR(ret = read(events[i].data.fd, msg, BUFSIZ));
					if(0 == ret)	living = false;
					else 			send(sockfd, msg, BUFSIZ, 0);
						
				}
			}
		}
		close(pipe_fd[0]);
		close(sockfd);
	}

	return 0;
}