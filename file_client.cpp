#include "utility.h"

#define SEND_FILE ("pushfile|target|png")

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
	int sockfd = init_client_socket(IP, PORT);
	int fd = open("target.png", O_RDONLY);
	
	struct stat stat_buf;
	fstat(fd, &stat_buf);

	CHK_ERROR(send(sockfd, SEND_FILE, sizeof(SEND_FILE), 0));
	
	usleep(1);	// if donot sleep, server cannot recv all data
	CHK_ERROR(sendfile(sockfd, fd, NULL, stat_buf.st_size));

	close(fd);
	close(sockfd);

	return 0;
}