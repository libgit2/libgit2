/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_netops_h__
#define INCLUDE_netops_h__

#include "posix.h"

typedef struct gitno_buffer {
	char *data;
	size_t len;
	size_t offset;
	GIT_SOCKET fd;
} gitno_buffer;

void gitno_buffer_setup(gitno_buffer *buf, char *data, unsigned int len, GIT_SOCKET fd);
int gitno_recv(gitno_buffer *buf);
void gitno_consume(gitno_buffer *buf, const char *ptr);
void gitno_consume_n(gitno_buffer *buf, size_t cons);

int gitno_connect(GIT_SOCKET *s, const char *host, const char *port);
int gitno_send(GIT_SOCKET s, const char *msg, size_t len, int flags);
int gitno_close(GIT_SOCKET s);
int gitno_send_chunk_size(int s, size_t len);
int gitno_select_in(gitno_buffer *buf, long int sec, long int usec);

int gitno_extract_host_and_port(char **host, char **port, const char *url, const char *default_port);

#endif
