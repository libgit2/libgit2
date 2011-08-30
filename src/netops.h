/*
 * netops.h - convencience functions for networking
 */
#ifndef INCLUDE_netops_h__
#define INCLUDE_netops_h__

#ifndef GIT_WIN32
typedef int GIT_SOCKET;
#else
typedef unsigned int GIT_SOCKET;
#endif

typedef struct gitno_buffer {
	char *data;
	size_t len;
	size_t offset;
	GIT_SOCKET fd;
} gitno_buffer;

void gitno_buffer_setup(gitno_buffer *buf, char *data, unsigned int len, int fd);
int gitno_recv(gitno_buffer *buf);
void gitno_consume(gitno_buffer *buf, const char *ptr);
void gitno_consume_n(gitno_buffer *buf, size_t cons);

int gitno_connect(const char *host, const char *port);
int gitno_send(int s, const char *msg, size_t len, int flags);
int gitno_select_in(gitno_buffer *buf, long int sec, long int usec);

#endif
