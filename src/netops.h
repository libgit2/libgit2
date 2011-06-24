/*
 * netops.h - convencience functions for networking
 */
#ifndef INCLUDE_netops_h__
#define INCLUDE_netops_h__

typedef struct gitno_buffer {
	void *data;
	unsigned int len;
	unsigned int offset;
	int fd;
} gitno_buffer;

void gitno_buffer_setup(gitno_buffer *buf, void *data, unsigned int len, int fd);
int gitno_recv(gitno_buffer *buf);
void gitno_consume(gitno_buffer *buf, void *ptr);
void gitno_consume_n(gitno_buffer *buf, unsigned int cons);

int gitno_connect(const char *host, const char *port);
int gitno_send(int s, const char *msg, int len, int flags);

#endif
