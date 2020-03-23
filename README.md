##The actual awesome "Libtask" by Russ Cox

#How it works

- There is a linkedlist of tasks(run queue), and taskscheduler picks the head and swaps to it's context.
- a task can yield, meaning it's put at the end of run-queue(to be executed at a later time) and the context switches to the scheduler

- Fd.c , is another "main task" specific for socket/read/write
- hold socket 'tasks' references in a sleeping queue (linked list datastructure) and also in the epoll_event(ev.data.ptr) datastructure,  and schedules them (taskready) when the task has some data on the socket
- When there is a call to read/write socket functions, we add them to epoll and also track the task
on a sleeping queue, when the main 'fd task' runs it epoll query for events to see if there are any data on the sockets and add them to taskready queue, for read and write there is a fd's using dup.


###Whats modified
-Added Multithreading support.
-Added Epoll instead of poll
-Added taskcreateraw() - creates a task and doesn't schedules it immediately unlike taskcreate()
-Removed debugging information by using build flag -DVERSION_DEBUG
-Ability to hook system calls (read, write, accept ..(more to come) ) to make your existing libraries like libSSL, mongo and mysql run on co-routines.
-More comments/documentation


