#ifndef WRAPSYSCALLS_H_
#define WRAPSYSCALLS_H_
#include <sys/types.h>
#include <sys/socket.h>

extern "C"{

	ssize_t __real_read(int fildes, void *buf, size_t nbyte);
	
	ssize_t __real_write(int fildes, const void *buf, size_t nbyte);

	int __real_connect(int sockfd, struct sockaddr *addr, socklen_t addrlen);	
	int __real_accept(int sockfd, struct sockaddr *clientaddr, socklen_t *addrlen);
	
	ssize_t __real_recv( int socket, void *buffer, size_t length, int flags );

	ssize_t __real_send(int socket, const void *buffer, size_t length, int flags);
	
	ssize_t __real_recvfrom(int socket, void *buffer, size_t length,
		                 int flags, struct sockaddr *address,
						               socklen_t *address_len);

	ssize_t __real_sendto(int socket, const void *message, size_t length,
		                 int flags, const struct sockaddr *dest_addr,
						               socklen_t dest_len);
}
	
#endif