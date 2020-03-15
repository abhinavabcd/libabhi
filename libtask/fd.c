
#include <sys/socket.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <resolv.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <netdb.h>

#include "taskimpl.h"



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


/*############################################################
	HOOK SYSTEM FUNCTIONS COPIED FROM Tencent/Libco
*/

typedef int (*socket_pfn_t)(int domain, int type, int protocol);
typedef int (*connect_pfn_t)(int socket, const struct sockaddr *address, socklen_t address_len);
typedef int (*close_pfn_t)(int fd);

typedef ssize_t (*read_pfn_t)(int fildes, void *buf, size_t nbyte);
typedef ssize_t (*write_pfn_t)(int fildes, const void *buf, size_t nbyte);

typedef ssize_t (*sendto_pfn_t)(int socket, const void *message, size_t length,
	                 int flags, const struct sockaddr *dest_addr,
					               socklen_t dest_len);

typedef ssize_t (*recvfrom_pfn_t)(int socket, void *buffer, size_t length,
	                 int flags, struct sockaddr *address,
					               socklen_t *address_len);

typedef size_t (*send_pfn_t)(int socket, const void *buffer, size_t length, int flags);
typedef ssize_t (*recv_pfn_t)(int socket, void *buffer, size_t length, int flags);

typedef int (*poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);
typedef int (*setsockopt_pfn_t)(int socket, int level, int option_name,
			                 const void *option_value, socklen_t option_len);

typedef int (*fcntl_pfn_t)(int fildes, int cmd, ...);

typedef struct tm *(*localtime_r_pfn_t)( const time_t *timep, struct tm *result );
typedef int (*setenv_pfn_t)(const char *name, const char *value, int overwrite);
typedef int (*unsetenv_pfn_t)(const char *name);
typedef char *(*getenv_pfn_t)(const char *name);

typedef hostent* (*gethostbyname_pfn_t)(const char *name);

typedef res_state (*__res_state_pfn_t)();

typedef int (*__poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);


static socket_pfn_t g_sys_socket_func 	= (socket_pfn_t)dlsym(RTLD_NEXT,"socket");
static connect_pfn_t g_sys_connect_func = (connect_pfn_t)dlsym(RTLD_NEXT,"connect");
static close_pfn_t g_sys_close_func 	= (close_pfn_t)dlsym(RTLD_NEXT,"close");

static read_pfn_t g_sys_read_func 		= (read_pfn_t)dlsym(RTLD_NEXT,"read");
static write_pfn_t g_sys_write_func 	= (write_pfn_t)dlsym(RTLD_NEXT,"write");

static sendto_pfn_t g_sys_sendto_func 	= (sendto_pfn_t)dlsym(RTLD_NEXT,"sendto");
static recvfrom_pfn_t g_sys_recvfrom_func = (recvfrom_pfn_t)dlsym(RTLD_NEXT,"recvfrom");

static send_pfn_t g_sys_send_func 		= (send_pfn_t)dlsym(RTLD_NEXT,"send");
static recv_pfn_t g_sys_recv_func 		= (recv_pfn_t)dlsym(RTLD_NEXT,"recv");

static poll_pfn_t g_sys_poll_func 		= (poll_pfn_t)dlsym(RTLD_NEXT,"poll");

static setsockopt_pfn_t g_sys_setsockopt_func 
										= (setsockopt_pfn_t)dlsym(RTLD_NEXT,"setsockopt");
static fcntl_pfn_t g_sys_fcntl_func 	= (fcntl_pfn_t)dlsym(RTLD_NEXT,"fcntl");

static setenv_pfn_t g_sys_setenv_func   = (setenv_pfn_t)dlsym(RTLD_NEXT,"setenv");
static unsetenv_pfn_t g_sys_unsetenv_func = (unsetenv_pfn_t)dlsym(RTLD_NEXT,"unsetenv");
static getenv_pfn_t g_sys_getenv_func   =  (getenv_pfn_t)dlsym(RTLD_NEXT,"getenv");
static __res_state_pfn_t g_sys___res_state_func  = (__res_state_pfn_t)dlsym(RTLD_NEXT,"__res_state");

static gethostbyname_pfn_t g_sys_gethostbyname_func = (gethostbyname_pfn_t)dlsym(RTLD_NEXT, "gethostbyname");

static __poll_pfn_t g_sys___poll_func = (__poll_pfn_t)dlsym(RTLD_NEXT, "__poll");


/*uncomment below printf if you just want to see hooked functions from some 3rd party libraries uses.*/
#define HOOK_SYS_FUNC(name) /*printf(#name);*/if( !g_sys_##name##_func ) { g_sys_##name##_func = (name##_pfn_t)dlsym(RTLD_NEXT,#name); }


/*############################################################*/






/*constants*/
const int _32K_ = 32*1024;

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
			taskready(t);
		}

		if(v && ((int (*)(void *))v)(_pool_loop_arg) ==-1  ){ //function pointer
			//wake up all sleeping tasks and mark them timeout before exiting
			while( (t= sleeping.head) !=NULL ){
				deltask(&sleeping, t);
				if(--sleepingcounted == 0)
					taskcount--;
				t->udata = (void *)(LIBTASK_POLL_TIMEOUT); // for fd based tasks you can use this data to store flags
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

static int epfd;

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
			_task_to_run->udata = 0; //reset this
			deltask(&sleeping, _task_to_run); //removing it from timeout queue

            taskready(_task_to_run);
		}

		now = nsec();
		while((t= sleeping.head) && now >= t->alarmtime){
			deltask(&sleeping, t);
			if(--sleepingcounted == 0)
				taskcount--;
			t->udata = (void *)(LIBTASK_POLL_TIMEOUT); // for fd based tasks you can use this data to store flags
			taskready(t);
		}

		if(v && ((int (*)(void *))v)(_pool_loop_arg) ==-1  ){ //function pointer
			//wake up all sleeping tasks and mark them timeout?
			while( (t= sleeping.head) !=NULL ){
				deltask(&sleeping, t);
				if(--sleepingcounted == 0)
					taskcount--;
				t->udata = (void *)(LIBTASK_POLL_TIMEOUT); // for fd based tasks you can use this data to store flags
				taskready(t);
			}

			break; // end this task
		}
	}
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


/* Like fdread but always calls fdwait before reading. */
ssize_t read1(size_t fd, void *buf, size_t n){
	int m;

	do
		fdwait(fd, 'r');
	while((m= g_sys_read_func(fd, buf, n)) < 0 && errno == EAGAIN);
	return m;
}

ssize_t read(size_t fd, void *buf, size_t n){

	HOOK_SYS_FUNC( read );

	int m;
	
	while((m= g_sys_read_func(fd, buf, n)) < 0 && errno == EAGAIN)
		fdwait(fd, 'r');
	return m;
}

ssize_t write(size_t fd, void *buf, size_t n){

	HOOK_SYS_FUNC( write );

	int m, tot;
	
	for(tot=0; tot<n; tot+=m){
		while((m= g_sys_write_func(fd, (char*)buf+tot, n-tot)) < 0 && errno == EAGAIN)
			fdwait(fd, 'w');
		if(m < 0)
			return m;
		if(m == 0)
			break;
	}
	return tot;
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


