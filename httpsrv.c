#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <mqueue.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define THREADS_COUNT	10

char *host = "127.0.0.1";
char *directory = "./";
int port = 12345;

char *mqueue_name = "/mqueue.mq";
mqd_t mqueue;

void http_client(int nd)
{
	char buf[1024];

	int size = recv(nd, buf, sizeof(buf), MSG_NOSIGNAL);
	if (size <= 0) {
		printf("recv: %d : %s\n", size, strerror(errno));
		return;
	}
	
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

		fclose(fd);

		snprintf(buf, sizeof(buf), "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n%s", strlen(fb), fb);
	}

	printf("send: %d\n%s\n", strlen(buf), buf);

	size = send(nd, buf, strlen(buf), MSG_NOSIGNAL);
	assert(size == strlen(buf));
}

void *thread_fn(void *data)
{
	int ret;
	int sd;

	while (1) {
		ret = mq_receive(mqueue, (char *)&sd, sizeof(sd), NULL);
		if (ret == sizeof(sd)) {
			http_client(sd);
			close(sd);
		}	
	}
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

	signal(SIGCHLD, SIG_IGN);

	daemon(1, 1);

	char file[128];
	snprintf(file, sizeof(file), "%s.access.log", argv[0]);
	int fd = open(file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	assert(fd >= 0);

	close(fileno(stdin));
	close(fileno(stdout));
	close(fileno(stderr));
	dup(fd);
	dup(fd);
	dup(fd);

	pthread_t id[THREADS_COUNT];
	int i;
	for (i = 0; i < THREADS_COUNT; ++i) {
		pthread_create(&id[i], NULL, thread_fn, NULL);
	}	

	struct mq_attr ma;
	ma.mq_flags = 0;                // blocking read/write
	ma.mq_maxmsg = 10;             	// maximum number of messages allowed in queue
	ma.mq_msgsize = sizeof(int);    		
	ma.mq_curmsgs = 0;              // number of messages currently in queue

	mq_unlink(mqueue_name);
	mqueue = mq_open(mqueue_name, O_RDWR | O_CREAT, 0660, &ma);
	assert(mqueue != -1);

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
		assert(nd != -1);
		assert(mq_send(mqueue, (const char *)&nd, sizeof(nd), 1) == 0);
	}

	close(sd);
	close(fd);	

	return 0;
}