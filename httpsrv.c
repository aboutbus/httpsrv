#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

char *host = "127.0.0.1";
char *directory = "./";
int port = 1080;

void http_client(int nd)
{
	char buf[1024];

	int size = recv(nd, buf, sizeof(buf), MSG_NOSIGNAL);
	assert(size >= 0);
	buf[size] = '\0';

	printf("recv: %d\n%s\n", size, buf);

	char file[128];
	strcpy(file, directory);
	int ret = sscanf(buf, "GET %s", file + strlen(file));
	assert(ret == 1);

	char *ptr = strchr(file, '?');
	if (ptr != '\0') {
		*ptr = '\0';
	}

	printf("file: %s\n\n", file);

	FILE *fd = fopen(file, "r");
	if (fd == NULL) {		
		
		snprintf(buf, sizeof(buf), "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
	} else {
		char fb[1024];
		size = fread(fb, 1, sizeof(fb), fd);
		assert(size >= 0);
		fb[size] = '\0';

		snprintf(buf, sizeof(buf), "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n%s", strlen(fb), fb);
	}

	printf("send: %d\n%s\n", strlen(buf), buf);

	size = send(nd, buf, strlen(buf), MSG_NOSIGNAL);
	assert(size == strlen(buf));
}

int main(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "h:p:d:")) != -1) {
		switch(opt) {
			case 'h':
				host = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'd':
				directory = optarg;
				break;
		}
	}

	daemon(1, 1);

	int fd = open("./access.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	assert(fd >= 0);

	close(fileno(stdin));
	close(fileno(stdout));
	close(fileno(stderr));
	dup(fd);
	dup(fd);
	dup(fd);

	printf("host: %s port: %d directory: %s\n", host, port, directory);

	int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert(sd != -1);

	int optval = 1;
	assert(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void *) &optval, (socklen_t) sizeof(optval)) != -1);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	inet_aton(host, &sa.sin_addr);
	
	assert(bind(sd, (struct sockaddr *)&sa, sizeof(sa)) != -1);	
	assert(listen(sd, SOMAXCONN) != -1);	

	while(1) {		
		int nd = accept(sd, NULL, 0);
		assert(sd != -1);

		int ret = fork();
		if (ret == 0) {
			http_client(nd);
			close(nd);			
			return 0;
		}
	}

	close(sd);

	return 0;
}