#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <task.h>
#include <stdlib.h>

char *server;
char *url;

void fetchtask(void*);

void
taskmain(int argc, char **argv)
{
	int i, n;
	
	if(argc != 4){
		fprintf(stderr, "usage: httpload n server url\n");
		taskexitall(1);
	}
	n = atoi(argv[1]);
	server = argv[2];
	url = argv[3];

	for(i=0; i<n; i++){
		taskcreate(fetchtask, 0, _8K_);
		while(taskyield() > 1)
			;
		sleep(1);
	}
}

void
fetchtask(void *v)
{
	int fd, n;
	char buf[512];
	
	fprintf(stderr, "starting...\n");
	for(;;){
		if((fd = netdial(TCP, server, 80)) < 0){
			fprintf(stderr, "dial %s: %s (%s)\n", server, strerror(errno), taskgetstate());
			continue;
		}
		snprintf(buf, sizeof buf, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", url, server);
		write(fd, buf, strlen(buf));
		while((n = read(fd, buf, sizeof buf)) > 0)
			;
		close(fd);
		write(1, ".", 1);
	}
}


int argc = 0;
char **argv = NULL;
void _taskmain(){
	taskmain(argc, argv);
}

int main(int _argc, char **_argv){
	argc = _argc;
	argv = _argv;
	taskcreate(_taskmain, 0, _32K_);
	taskscheduler();
	return 0;
}
