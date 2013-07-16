/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_netops_h__
#define INCLUDE_netops_h__

#include "posix.h"
#include "common.h"

#ifdef GIT_SSL
# include <openssl/ssl.h>
#endif

typedef struct gitno_ssl_t {
#ifdef GIT_SSL
	SSL_CTX *ctx;
	SSL *ssl;
#else
	size_t dummy;
#endif
} gitno_ssl;

/* Represents a socket that may or may not be using SSL */
typedef struct gitno_socket_t {
	GIT_SOCKET socket;
	gitno_ssl ssl;
} gitno_socket;

typedef struct gitno_buffer_t gitno_buffer;
struct gitno_buffer_t {
	char *data;
	size_t len;
	size_t offset;
	gitno_socket *socket;
	int (*recv)(gitno_buffer *buffer);
	void *cb_data;
};

/* Flags to gitno_connect */
enum {
	/* Attempt to create an SSL connection. */
	GITNO_CONNECT_SSL = 1,

	/* Valid only when GITNO_CONNECT_SSL is also specified.
	 * Indicates that the server certificate should not be validated. */
	GITNO_CONNECT_SSL_NO_CHECK_CERT = 2,
};

void gitno_buffer_setup(gitno_socket *t, gitno_buffer *buf, char *data, size_t len);
void gitno_buffer_setup_callback(gitno_socket *t, gitno_buffer *buf, char *data, size_t len, int (*recv)(gitno_buffer *buf), void *cb_data);
int gitno_recv(gitno_buffer *buf);

void gitno_consume(gitno_buffer *buf, const char *ptr);
void gitno_consume_n(gitno_buffer *buf, size_t cons);

int gitno_connect(gitno_socket *socket, const char *host, const char *port, int flags);
int gitno_send(gitno_socket *socket, const char *msg, size_t len, int flags);
int gitno_close(gitno_socket *s);
int gitno_select_in(gitno_buffer *buf, long int sec, long int usec);

int gitno_extract_url_parts(
		char **host,
		char **port,
		char **username,
		char **password,
		const char *url,
		const char *default_port);

#endif
