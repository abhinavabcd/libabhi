#include <fcntl.h>

#include "taskimpl.h"
#include "wrap_syscalls.h"


/*Queue that keeps track of sleeping tasks*/
__thread Tasklist sleeping = {0};

/*
  all sleeping tasks are counted a single task, 
  funny but keeps taskscount to a cleaner value
*/
__thread int sleepingcounted = 0;

/*internal variable indicating whether we have started the fd task*/
__thread int startedfdtask;

/* forward delcaring nano seconds*/
inline size_t nsec(void);

/* used with loop function */
__thread void *_pool_loop_arg;



/*switch to poll implementation if not linux*/
#ifndef __linux__
#include <sys/poll.h>

enum {
	MAXFD = 1024
};

static struct pollfd pollfd[MAXFD];
static Task *polltask[MAXFD];
static int npollfd;

void fdtask(void *v){
	int i, ms;
	Task *t;
	uvlong now;
	
	TASKNAME("fdtask");
	for(;;){
		/* let everyone else run */
		while(taskyield() > 0);
		/* we're the only one runnable - poll for i/o */
		errno = 0;
		TASKSTATE("poll");
		if((t=sleeping.head) == NULL)
			ms = 1000; // just wait for 1 sec
		else{
			/* sleep at most 5s */
			now = nsec();
			if(now >= t->alarmtime)
				ms = 0;
			else if(now+5*1000*1000*1000LL >= t->alarmtime)
				ms = (t->alarmtime - now)/1000000;
			else
				ms = 5000;
		}
		if(poll(pollfd, npollfd, ms) < 0){
			if(errno == EINTR)
				continue;
			fprint(2, "poll: %s\n", strerror(errno));
			taskexitall(0);
		}

		/* wake up the guys who deserve it */
		for(i=0; i<npollfd; i++){
			while(i < npollfd && pollfd[i].revents){
				taskready(polltask[i]);
				--npollfd;
				pollfd[i] = pollfd[npollfd];
				polltask[i] = polltask[npollfd];
			}
		}
		
		now = nsec();
		while((t=sleeping.head) && now >= t->alarmtime){
			deltask(&sleeping, t);
			if(--sleepingcounted == 0)
				taskcount--;
			t->udata = (void *)(ETIMEDOUT); // for fd based tasks you can use this data to store flags
			taskready(t);
		}

		if(v && ((int (*)(void *))v)(_pool_loop_arg) ==-1  ){ //function pointer
			//wake up all sleeping tasks and mark them timeout before exiting
			while( (t= sleeping.head) !=NULL ){
				deltask(&sleeping, t);
				if(--sleepingcounted == 0)
					taskcount--;
				t->udata = (void *)(EIO); // for fd based tasks you can use this data to store flags
				taskready(t);
			}

			break; // end this task
		}

	}
}

void fdwait(int fd, int rw){
	int bits;

	if(!startedfdtask){
		startedfdtask = 1;
		taskcreate(fdtask, 0, _32K_);
	}

	if(npollfd >= MAXFD){
		fprint(2, "too many poll file descriptors\n");
		abort();
	}
	
	TASKSTATE("fdwait for %s", rw=='r' ? "read" : rw=='w' ? "write" : "error");
	bits = 0;
	switch(rw){
	case 'r':
		bits |= POLLIN;
		break;
	case 'w':
		bits |= POLLOUT;
		break;
	}

	polltask[npollfd] = taskrunning;
	pollfd[npollfd].fd = fd;
	pollfd[npollfd].events = bits;
	pollfd[npollfd].revents = 0;
	npollfd++;

    taskdelay(25000); // add this task a sleeping queue
}

#else 

/* Linux - switch to epoll */
#include <sys/epoll.h>

__thread static int epfd;

void fdtask(void *v){
	int i, ms;
	Task *t;
	uvlong now;

	TASKNAME("fdtask");
    struct epoll_event events[1000];
	for(;;){
		/* 
			let everyone else run because this just blocks 
			for 1sec if there are no events on fd's and 
			it will effectly waste other tasks time
		*/
		while(taskyield() > 0);
		/* we're the only one runnable - poll for i/o */
		errno = 0;
		TASKSTATE("epoll");
		if((t= sleeping.head) == NULL){
			ms = 1000; // just wait for 1 sec
		}
		else{
			/* sleep at most 25s */
			now = nsec();
			if(now >= t->alarmtime)
				ms = 0;
			else if(now+25*1000*1000*1000LL >= t->alarmtime)
				ms = (t->alarmtime - now)/1000000;
			else
				ms = 25 * 1000;
		}

        int nevents;
		if((nevents = epoll_wait(epfd, events, 1000, ms)) < 0){
			if(errno == EINTR)
				continue;
			fprint(2, "epoll: %s\n", strerror(errno));
			taskexitall(0);
		}

		/* wake up the guys who deserve it and put them to scheduler queue*/
		for(i=0; i<nevents; i++){
			Task *_task_to_run = (Task *)events[i].data.ptr;
			_task_to_run->udata = 0; //reset this to no errno
			deltask(&sleeping, _task_to_run); //removing it from timeout queue

            taskready(_task_to_run);
		}

		now = nsec();
		while((t= sleeping.head) && now >= t->alarmtime){
			deltask(&sleeping, t);
			if(--sleepingcounted == 0)
				taskcount--;
			t->udata = (void *)(ETIMEDOUT); // for fd based tasks you can use this data to store flags
			taskready(t);
		}

		//destroy and cleanup
		if(v && ((int (*)(void *))v)(_pool_loop_arg) ==-1  ){ //function pointer
			//wake up all sleeping tasks and mark them timeout?
			while( (t= sleeping.head) !=NULL ){
				deltask(&sleeping, t);
				if(--sleepingcounted == 0)
					taskcount--;
				t->udata = (void *)(EIO); // for fd based tasks you can use this data to store flags
				taskready(t);
			}

			break; // end this task
		}
	}
	startedfdtask = 0;
}

/*
  just adds fd to epoll and switches to scheduler 
  epoll will try to revive the task and add it to run queue again
*/
void fdwait(int fd, int rw){

	if(!startedfdtask){
		startedfdtask = 1;
        epfd = epoll_create(1);
        assert(epfd >= 0);
		taskcreate(fdtask, 0, _32K_);
	}

	TASKSTATE("fdwait for %s", rw=='r' ? "read" : rw=='w' ? "write" : "error");
    struct epoll_event ev = {0};
    ev.data.ptr = taskrunning;
	switch(rw){
		case 'r':
			ev.events |= EPOLLIN | EPOLLPRI | EPOLLERR;
			break;
		case 'w':
			ev.events |= EPOLLOUT | EPOLLERR;
			break;
	}

    int r = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    int duped = 0;
    if (r < 0 && errno == EEXIST) {
        duped = 1;
        fd = dup(fd);
        int r = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        assert(r == 0);
    }

    taskdelay(25000); // taskswitch();// add to epoll and goes to scheduler(Note: we are not even on the scheduler queue, let epoll take care of it)
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);
    if (duped)
        close(fd);
}

#endif 


/* You can start this function that execute the callback on every loop */
void startfdtask(int (*fn)(void *), void *arg){

	_pool_loop_arg = arg;
	if(!startedfdtask){
		startedfdtask = 1;
#ifdef __linux__
        epfd = epoll_create(1);
        assert(epfd >= 0);
#endif
		taskcreate(fdtask, (void *)fn, _32K_);
	}	
}

/* add current task to sleeping queue and switch back to scheduler */
size_t taskdelay(size_t ms){
	size_t when, now;
	Task *t;
	
	if(!startedfdtask){
		startedfdtask = 1;
#ifdef __linux__
        epfd = epoll_create(1);
        assert(epfd >= 0);
#endif
		taskcreate(fdtask, 0, _32K_);
	}

	now = nsec();
	when = now + ms*1000000;
	for(t=sleeping.tail; t!=NULL && t->alarmtime >= when; t=t->prev);//iterate through tasks until you find the right place

	if(t){
		taskrunning->prev = t->prev;
		taskrunning->next = t;
	}else{
		taskrunning->prev = sleeping.tail;
		taskrunning->next = NULL;
	}

	t = taskrunning;
	t->alarmtime = when;
	if(t->prev)
		t->prev->next = t;
	else
		sleeping.head = t;
	if(t->next)
		t->next->prev = t;
	else
		sleeping.tail = t;

	if(sleepingcounted++ == 0){ //if we have any sleeping tasks they mark as 
		taskcount++;
	}
	taskswitch(); 

	return (nsec() - now)/1000000;
}


extern "C" {
	/* Wrap system calls */

	ssize_t __wrap_read(size_t fd, void *buf, size_t n){


		int m;
		
		while((m= __real_read(fd, buf, n)) < 0 && errno == EAGAIN){
			int _err = (int)((intptr_t) taskdata()); // if we marked any errors (like timeout, closed)
			if(_err){
				errno = _err;
				return -1;
			}

			fdwait(fd, 'r');
		}

		return m;
	}

	ssize_t __wrap_write(size_t fd, void *buf, size_t n){

		int m, tot;
		
		for(tot=0; tot<n; tot+=m){
			while((m= __real_write(fd, (char*)buf+tot, n-tot)) < 0 && errno == EAGAIN){

				//check for custom errno that we set (mainly our own timeout)
				int _err = (int)((intptr_t) taskdata() );
				if(_err){
					errno = _err;
					return -1;
				}

				fdwait(fd, 'w');
			}
			if(m < 0)
				return m;
			if(m == 0)
				break;
		}

		return tot;
	}

}



/* Like fdread but always calls fdwait before reading. */
ssize_t read1(size_t fd, void *buf, size_t n){
	int m;

	do{
		//mark any custom errno
		int _err = (int)((intptr_t) taskdata() ); // if we marked any errors (like timeout, closed)
		if(_err){
			errno = _err;
			return -1;
		}
		fdwait(fd, 'r');
	}while((m= __real_read(fd, buf, n)) < 0 && errno == EAGAIN);

	return m;
}



inline ssize_t	fdread(size_t fd, void *buf, size_t n){
	return	__wrap_read(fd, buf, n);
}

inline ssize_t	fdwrite(size_t fd, void *buf, size_t n){
	return __wrap_write(fd, buf, n);
}

int fdnoblock(int fd){
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_NONBLOCK);
}

inline size_t nsec(void){
	struct timeval tv;

	if(gettimeofday(&tv, 0) < 0)
		return -1;
	return (size_t)tv.tv_sec*1000*1000*1000 + tv.tv_usec*1000;
}

