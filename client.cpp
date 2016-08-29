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
	pid_t pid;
	int sockfd = make_client_socket(IP, PORT);
	int epfd, event_cnt, pipe_fd[2];
	char msg[BUFSIZ]; bzero(msg, BUFSIZ);

	Try(pipe(pipe_fd))

	Try(epfd = epoll_create(EPOLL_SIZE))

	struct epoll_event events[2];
	epfd_add(epfd, sockfd, true);
	epfd_add(epfd, pipe_fd[0], true);

	Try(pid = fork())
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
				Try(write(pipe_fd[1], msg, strlen(msg)-1))
		}
		close(pipe_fd[1]);
	}
	else 				// 父进程 read 0
	{
		close(pipe_fd[1]);
		while(living)
		{
			Try(event_cnt = epoll_wait(epfd, events, EPOLL_SIZE, -1))
			for(int i = 0; i < event_cnt; i++)
			{
				int ret;
				bzero(msg, BUFSIZ);
				if(sockfd == events[i].data.fd)
				{
					Try(recv(sockfd, msg, BUFSIZ, 0));
					if(0 == ret)	living = false;
					else 			cout<<msg<<endl;	
				}
				else 	// read msg from stdin
				{
					Try(ret = read(events[i].data.fd, msg, BUFSIZ));
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