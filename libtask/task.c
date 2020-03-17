/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#include "taskimpl.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/syscall.h>

/*inidicates number of runnings tasks*/
__thread int	taskcount;

/*internal number increments on each scheduler switch*/
__thread int	_tasknswitch;

__thread int	taskexitval;

/*indicates current running task*/
__thread Task	*taskrunning;

/*scheduler context, whenever you want to switch to scheduler use this*/
__thread Context	taskschedcontext;

/* main running tasks queue*/
__thread Tasklist	taskrunqueue;

/* tracks all malloc'ed tasks here */
__thread Task	**alltask;
__thread int	nalltask;

static char *argv0;
static	void		contextswitch(Context *from, Context *to);

void taskdebug(char *fmt, ...){
	va_list arg;
	char buf[128];
	Task *t;
	char *p;
	static int fd = -1;
	va_start(arg, fmt);
	vfprint(1, fmt, arg);
	va_end(arg);
return;

	if(fd < 0){
		p = strrchr(argv0, '/');
		if(p)
			p++;
		else
			p = argv0;
		snprint(buf, sizeof buf, "/tmp/%s.tlog", p);
		if((fd = open(buf, O_CREAT|O_WRONLY, 0666)) < 0)
			fd = open("/dev/null", O_WRONLY);
	}

	va_start(arg, fmt);
	vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	t = taskrunning;
	if(t)
		fprint(fd, "%d.%d: %s\n", getpid(), t->id, buf);
	else
		fprint(fd, "%d._: %s\n", getpid(), buf);
}

static void taskstart(uint y, uint x) {
	Task *t;
	ulong z;

	z = x<<16;	/* hide undefined 32-bit shift from 32-bit compilers */
	z <<= 16;
	z |= y;
	t = (Task*)z;

//print("taskstart %p\n", t);
	t->startfn(t->startarg);
//print("taskexits %p\n", t);
	taskexit(0);
//print("not reacehd\n");
}

static int taskidgen;

static Task* taskalloc(void (*fn)(void*), void *arg, uint stack){
	Task *t;
	sigset_t zero;
	uint x, y;
	ulong z;

	/* allocate the task and stack together */
	t = (Task *)malloc(sizeof *t+stack);
	if(t == NULL){
		fprint(2, "taskalloc malloc: %r\n");
		abort();
	}
	memset(t, 0, sizeof *t);
	t->stk = (uchar*)(t+1);
	t->stksize = stack;
	t->id = ++taskidgen;
	t->startfn = fn;
	t->startarg = arg;

	/* do a reasonable initialization */
	memset(&t->context.uc, 0, sizeof t->context.uc);
	sigemptyset(&zero);
	sigprocmask(SIG_BLOCK, &zero, &t->context.uc.uc_sigmask);

	/* must initialize with current context */
	if(getcontext(&t->context.uc) < 0){
		fprint(2, "getcontext: %r\n");
		abort();
	}

	/* call makecontext to do the real work. */
	/* leave a few words open on both ends */
	t->context.uc.uc_stack.ss_sp = t->stk+8;
	t->context.uc.uc_stack.ss_size = t->stksize-64;
#if defined(__sun__) && !defined(__MAKECONTEXT_V2_SOURCE)		/* sigh */
#warning "doing sun thing"
	/* can avoid this with __MAKECONTEXT_V2_SOURCE but only on SunOS 5.9 */
	t->context.uc.uc_stack.ss_sp = 
		(char*)t->context.uc.uc_stack.ss_sp
		+t->context.uc.uc_stack.ss_size;
#endif
	/*
	 * All this magic is because you have to pass makecontext a
	 * function that takes some number of word-sized variables,
	 * and on 64-bit machines pointers are bigger than words.
	 */
//print("make %p\n", t);
	z = (ulong)t;
	y = z;
	z >>= 16;	/* hide undefined 32-bit shift from 32-bit compilers */
	x = z>>16;
	makecontext(&t->context.uc, (void(*)())taskstart, 2, y, x);

	return t;
}

Task* taskcreateraw(void (*fn)(void*), void *arg, uint stack){
	Task *t;
	t = taskalloc(fn, arg, stack);
	taskcount++;
	if(nalltask%64 == 0){
		alltask = (Task **)realloc(alltask, (nalltask+64)*sizeof(alltask[0]));
		if(alltask == NULL){
			fprint(2, "out of memory\n");
			abort();
		}
	}
	t->alltaskslot = nalltask;
	alltask[nalltask++] = t;
	return t;
}

int taskcreate(void (*fn)(void*), void *arg, uint stack){
	Task *t = taskcreateraw(fn, arg, stack);
	taskready(t);
	return t->id;
}
/*switch to scheduler, don't call this directly without putting to queue, otherwise the context is lost forever*/
void taskswitch(void){
	needstack(0);
	contextswitch(&taskrunning->context, &taskschedcontext);
}

void taskready(Task *t){
	if(t->_state == TASK_STATE_READY){
		return;// already in queue so ignore
	}
	t->_state = TASK_STATE_READY;
	addtask(&taskrunqueue, t);
}

/*
	put back into queue at the end, basically to try again later
	returns the number of tasks executed before it
*/
int taskyield(void){
	int n = _tasknswitch;
	taskready(taskrunning);
	TASKSTATE("yield");
	taskswitch();
	return _tasknswitch - n - 1;
}

int anyready(void){
	return taskrunqueue.head != NULL;
}

void taskexitall(int val){
	exit(val);
}

void taskexit(int val){
	taskexitval = val;
	taskrunning->_state = TASK_STATE_EXITING;
	taskswitch();
}

static void contextswitch(Context *from, Context *to){
	if(swapcontext(&from->uc, &to->uc) < 0){
		fprint(2, "swapcontext failed: %r\n");
		assert(0);
	}
}

void taskscheduler(void){
	int i;
	Task *t;

	TASKDEBUG("scheduler enter");
	for(;;){
		if(taskcount == 0)
			break;
		t = taskrunqueue.head;
		if(t == NULL){
			fprint(2, "no runnable tasks! %d tasks stalled\n", taskcount);
			break;
		}
		deltask(&taskrunqueue, t); // remove it from the queue
		t->_state = 0;
		taskrunning = t;
		_tasknswitch++;
		TASKDEBUG("run %d, %d (%s)\n",syscall(__NR_gettid), t->id, t->name);
		//switch to that task on queue
		contextswitch(&taskschedcontext, &t->context);
		//print("back in scheduler\n");
		taskrunning = NULL;
		if(t->_state == TASK_STATE_EXITING){
			taskcount--;
			i = t->alltaskslot;
			alltask[i] = alltask[--nalltask];
			alltask[i]->alltaskslot = i;
			free(t);
		}
	}
	/*free all tasks ourself?, or let the application hanle the cleanup ?*/
	while(nalltask > 0){
		free(alltask[--nalltask]);
	}
}

void* taskdata(void){
	return taskrunning ? taskrunning->udata : NULL;
}


void needstack(int n){
	Task *t;

	t = taskrunning;

	if((char*)&t <= (char*)t->stk
	|| (char*)&t - (char*)t->stk < 256+n){
		fprint(2, "task stack overflow: &t=%p tstk=%p n=%d\n", &t, t->stk, 256+n);
		abort();
	}
}


#ifdef VERSION_DEBUG
/*
 * debugging
 */
void taskname(char *fmt, ...){
	va_list arg;
	Task *t;

	t = taskrunning;
	va_start(arg, fmt);
	vsnprint(t->name, sizeof t->name, fmt, arg);
	va_end(arg);
}

char* taskgetname(void){
	return taskrunning->name;
}

void taskstate(char *fmt, ...){
	va_list arg;
	Task *t;

	t = taskrunning;
	va_start(arg, fmt);
	vsnprint(t->state, sizeof t->name, fmt, arg);
	va_end(arg);
}

char* taskgetstate(void){
	return taskrunning->state;
}


static void taskinfo(int s){
	int i;
	Task *t;
	char *extra;

	fprint(2, "task list:\n");
	for(i=0; i<nalltask; i++){
		t = alltask[i];
		if(t == taskrunning)
			extra = " (running)";
		else if(t->_state == TASK_STATE_READY)
			extra = " (ready)";
		else
			extra = "";
		fprint(2, "%6d %-20s %s%s\n", 
			t->id, 
			t->name, t->state, extra);
	}
}

#endif

/*
 * hooray for linked lists
 */
void addtask(Tasklist *l, Task *t){
	if(l->tail){
		l->tail->next = t;
		t->prev = l->tail;
	}else{
		l->head = t;
		t->prev = NULL;
	}
	l->tail = t;
	t->next = NULL;
}

void deltask(Tasklist *l, Task *t){
	if(t->prev)
		t->prev->next = t->next;
	else
		l->head = t->next;
	if(t->next)
		t->next->prev = t->prev;
	else
		l->tail = t->prev;
}

size_t taskid(void) {
	return taskrunning->id;
}
