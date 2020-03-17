/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <task.h>

int quiet;
int goal;
int buffer;

void
primetask(void *arg)
{
	Channel *c, *nc;
	int p, i;
	c = (Channel *)arg;

	p = chanrecvul(c);
	if(p > goal)
		taskexitall(0);
	if(!quiet)
		printf("%d\n", p);
	nc = chancreate(sizeof(unsigned long), buffer);
	taskcreate(primetask, nc, 32768);
	for(;;){
		i = chanrecvul(c);
		if(i%p)
			chansendul(nc, i);
	}
}

void
taskmain(int argc, char **argv)
{
	int i;
	Channel *c;

	if(argc>1)
		goal = atoi(argv[1]);
	else
		goal = 100;
	printf("goal=%d\n", goal);

	c = chancreate(sizeof(unsigned long), buffer);
	taskcreate(primetask, c, 32768);
	for(i=2;; i++)
		chansendul(c, i);
}

int argc = 0;
char **argv = NULL;
void _taskmain(void *arg){
	taskmain(argc, argv);
}

int main(int _argc, char **_argv){
	argc = _argc;
	argv = _argv;
	taskcreate(_taskmain, 0, _32K_);
	taskscheduler();
	return 0;
}

void*
emalloc(unsigned long n)
{
	return calloc(n ,1);
}

long
lrand(void)
{
	return rand();
}
