#include "utility.h"

#define SEND_FILE ("pushfile|target|png")

int main(int ac, char *av[])
{
	int sockfd = make_client_socket(IP, PORT);
	int fd = open("target.png", O_RDONLY);
	
	struct stat stat_buf;
	fstat(fd, &stat_buf);

	send(sockfd, SEND_FILE, sizeof(SEND_FILE), 0);
	
	usleep(1);	// if donot sleep, server cannot recv all data
	Try(sendfile(sockfd, fd, NULL, stat_buf.st_size))

	close(fd);
	close(sockfd);

	return 0;
}