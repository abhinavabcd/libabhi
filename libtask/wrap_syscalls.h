#ifndef WRAPSYSCALLS_H_
#define WRAPSYSCALLS_H_
#include <sys/types.h>

extern "C"{
	ssize_t __real_read(int fildes, void *buf, size_t nbyte);
	ssize_t __real_write(int fildes, const void *buf, size_t nbyte);
}
	
#endif