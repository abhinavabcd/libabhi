/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#ifndef _TASK_H_
#define _TASK_H_ 1

#include <stdarg.h>
#include <sys/types.h>
#include <inttypes.h>

/*
 * basic procs and threads
 */

typedef struct Task Task;
typedef struct Tasklist Tasklist;

int		anyready(void);
int		taskcreate(void (*f)(void *arg), void *arg, unsigned int stacksize);
void	taskexit(int);
void	taskexitall(int);
void	taskmain(int argc, char *argv[]);
int		taskyield(void);
void* 	taskdata(void);
void	needstack(int);

#ifdef VERSION_DEBUG
void		taskname(char*, ...);
void		taskstate(char*, ...);
char*		taskgetname(void);
char*		taskgetstate(void);
void 		taskdebug(char *fmt, ...);

#endif

size_t	taskdelay(size_t);
size_t	taskid(void);

struct Tasklist	/* used internally */
{
	Task	*head;
	Task	*tail;
};

/*
 * queuing locks
 */
typedef struct QLock QLock;
struct QLock
{
	Task	*owner;
	Tasklist waiting;
};

void	qlock(QLock*);
int	canqlock(QLock*);
void	qunlock(QLock*);

/*
 * reader-writer locks
 */
typedef struct RWLock RWLock;
struct RWLock
{
	int	readers;
	Task	*writer;
	Tasklist rwaiting;
	Tasklist wwaiting;
};

void	rlock(RWLock*);
int	canrlock(RWLock*);
void	runlock(RWLock*);

void	wlock(RWLock*);
int	canwlock(RWLock*);
void	wunlock(RWLock*);

/*
 * sleep and wakeup (condition variables)
 */
typedef struct Rendez Rendez;

struct Rendez
{
	QLock	*l;
	Tasklist waiting;
};

void	tasksleep(Rendez*);
int	taskwakeup(Rendez*);
int	taskwakeupall(Rendez*);

/*
 * channel communication
 */
typedef struct Alt Alt;
typedef struct Altarray Altarray;
typedef struct Channel Channel;

enum
{
	CHANEND,
	CHANSND,
	CHANRCV,
	CHANNOP,
	CHANNOBLK,
};

struct Alt
{
	Channel		*c;
	void		*v;
	unsigned int	op;
	Task		*task;
	Alt		*xalt;
};

struct Altarray
{
	Alt		**a;
	unsigned int	n;
	unsigned int	m;
};

struct Channel
{
	unsigned int	bufsize;
	unsigned int	elemsize;
	unsigned char	*buf;
	unsigned int	nbuf;
	unsigned int	off;
	Altarray	asend;
	Altarray	arecv;
	char		*name;
};

int		chanalt(Alt *alts);
Channel*	chancreate(int elemsize, int elemcnt);
void		chanfree(Channel *c);
int		chaninit(Channel *c, int elemsize, int elemcnt);
int		channbrecv(Channel *c, void *v);
void*		channbrecvp(Channel *c);
unsigned long	channbrecvul(Channel *c);
int		channbsend(Channel *c, void *v);
int		channbsendp(Channel *c, void *v);
int		channbsendul(Channel *c, unsigned long v);
int		chanrecv(Channel *c, void *v);
void*		chanrecvp(Channel *c);
unsigned long	chanrecvul(Channel *c);
int		chansend(Channel *c, void *v);
int		chansendp(Channel *c, void *v);
int		chansendul(Channel *c, unsigned long v);

/*
 * Threaded I/O.
*/

/*hooked sys functions*/
ssize_t		read(size_t, void*, size_t);
ssize_t		read1(size_t, void*, size_t);	/* always uses fdwait */
ssize_t		write(size_t, void*, size_t);

void		fdwait(int, int);
int			fdnoblock(int);

void		fdtask(void*);


/*this will spin the fd task optionally with arguments that gets called on each epoll iteration */
void 		startfdtask(int (*fn)(void *), void *arg);

/*
 * Network dialing - sets non-blocking automatically
 */
enum
{
	UDP = 0,
	TCP = 1,
};

int		netannounce(int, char*, int);
int		netaccept(int, char*, int*);
ssize_t		netdial(int, char*, int);
int		netlookup(char*, uint32_t*);	/* blocks entire program! */


enum{
	LIBTASK_POLL_TIMEOUT = 1989
};

#ifdef VERSION_DEBUG 
	#define TASKSTATE(...) taskstate(__VA_ARGS__)
	#define TASKNAME(...) taskname(__VA_ARGS__)
	#define TASKDEBUG(...) taskdebug(__VA_ARGS__)
#else
	#define TASKSTATE(...) 
	#define TASKNAME(...)
	#define TASKDEBUG(...)
#endif


#endif

