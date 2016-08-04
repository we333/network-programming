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

int main(int ac, char *av[])
{
	bool living = true;
	int sockfd = make_client_socket(IP, PORT);
	int epfd, event_cnt, pipe_fd[2];
	char msg[BUFSIZ]; bzero(msg, BUFSIZ);

	if(-1 == pipe(pipe_fd))
		myErr("pipe failed");

	if(-1 == (epfd = epoll_create(EPOLL_SIZE)))
		myErr("create epoll failed");

	struct epoll_event events[2];
	epfd_add(epfd, sockfd, true);
	epfd_add(epfd, pipe_fd[0], true);

	pid_t pid = fork();

	if(0 > pid)
	{
		myErr("fork failed");
	}
	else if(0 == pid)	// 子进程 write 1
	{
		close(pipe_fd[0]);
		while(living)
		{
			fgets(msg, BUFSIZ, stdin);
			if(0 == strncasecmp(msg, "exit", strlen("exit")))
				living = false;
			else
			{
				if(-1 == write(pipe_fd[1], msg, strlen(msg)-1))
					myErr("write failed");
			}
		}
		close(pipe_fd[1]);
	}
	else 				// 父进程 read 0
	{
		close(pipe_fd[1]);
		while(living)
		{
			if(-1 == (event_cnt = epoll_wait(epfd, events, EPOLL_SIZE, -1)))
				myErr("epoll wait failed");

			for(int i = 0; i < event_cnt; i++)
			{
				bzero(msg, BUFSIZ);
				if(sockfd == events[i].data.fd)
				{
					int ret = recv(sockfd, msg, BUFSIZ, 0);
					if(-1 == ret)	
					{
						myErr("recv failed");
					}
					else if(0 == ret)
					{
						cout<<"server closed"<<endl;
						living = false;
					}
					else
						cout<<msg<<endl;
				}
				else 	// read msg from stdin
				{
					int ret = read(events[i].data.fd, msg, BUFSIZ);
					if(0 == ret)	living = false;
					else
						send(sockfd, msg, BUFSIZ, 0);
				}
			}
		}
		close(pipe_fd[0]);
		close(sockfd);
	}

	return 0;
}