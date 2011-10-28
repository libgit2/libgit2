/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef _WIN32
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <sys/select.h>
#	include <netdb.h>
#else
#	define _WIN32_WINNT 0x0501
#	include <winsock2.h>
#	include <Ws2tcpip.h>
#	ifdef _MSC_VER
#		pragma comment(lib, "Ws2_32.lib")
#	endif
#endif


#include "git2/errors.h"

#include "common.h"
#include "netops.h"
#include "posix.h"

void gitno_buffer_setup(gitno_buffer *buf, char *data, unsigned int len, int fd)
{
	memset(buf, 0x0, sizeof(gitno_buffer));
	memset(data, 0x0, len);
	buf->data = data;
	buf->len = len;
	buf->offset = 0;
	buf->fd = fd;
}

int gitno_recv(gitno_buffer *buf)
{
	int ret;

	ret = recv(buf->fd, buf->data + buf->offset, buf->len - buf->offset, 0);
	if (ret < 0)
		return git__throw(GIT_EOSERR, "Failed to receive data: %s", strerror(errno));
	if (ret == 0) /* Orderly shutdown, so exit */
		return GIT_SUCCESS;

	buf->offset += ret;

	return ret;
}

/* Consume up to ptr and move the rest of the buffer to the beginning */
void gitno_consume(gitno_buffer *buf, const char *ptr)
{
	size_t consumed;

	assert(ptr - buf->data >= 0);
	assert(ptr - buf->data <= (int) buf->len);

	consumed = ptr - buf->data;

	memmove(buf->data, ptr, buf->offset - consumed);
	memset(buf->data + buf->offset, 0x0, buf->len - buf->offset);
	buf->offset -= consumed;
}

/* Consume const bytes and move the rest of the buffer to the beginning */
void gitno_consume_n(gitno_buffer *buf, size_t cons)
{
	memmove(buf->data, buf->data + cons, buf->len - buf->offset);
	memset(buf->data + cons, 0x0, buf->len - buf->offset);
	buf->offset -= cons;
}

int gitno_connect(const char *host, const char *port)
{
	struct addrinfo *info, *p;
	struct addrinfo hints;
	int ret, error = GIT_SUCCESS;
	GIT_SOCKET s;

	memset(&hints, 0x0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(host, port, &hints, &info);
	if (ret != 0) {
		error = GIT_EOSERR;
		info = NULL;
		goto cleanup;
	}

	for (p = info; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#ifdef GIT_WIN32
		if (s == INVALID_SOCKET) {
#else
		if (s < 0) {
#endif
			error = GIT_EOSERR;
			goto cleanup;
		}

		ret = connect(s, p->ai_addr, p->ai_addrlen);
		/* If we can't connect, try the next one */
		if (ret < 0) {
			continue;
		}

		/* Return the socket */
		error = s;
		goto cleanup;
	}

	/* Oops, we couldn't connect to any address */
	error = git__throw(GIT_EOSERR, "Failed to connect: %s", strerror(errno));

cleanup:
	freeaddrinfo(info);
	return error;
}

int gitno_send(GIT_SOCKET s, const char *msg, size_t len, int flags)
{
	int ret;
	size_t off = 0;

	while (off < len) {
		errno = 0;

		ret = send(s, msg + off, len - off, flags);
		if (ret < 0)
			return git__throw(GIT_EOSERR, "Error sending data: %s", strerror(errno));

		off += ret;
	}

	return off;
}


#ifdef GIT_WIN32
int gitno_close(GIT_SOCKET s)
{
	return closesocket(s) == SOCKET_ERROR ? -1 : 0;
}
#else
int gitno_close(GIT_SOCKET s)
{
	return close(s);
}
#endif

int gitno_select_in(gitno_buffer *buf, long int sec, long int usec)
{
	fd_set fds;
	struct timeval tv;

	tv.tv_sec = sec;
	tv.tv_usec = usec;

	FD_ZERO(&fds);
	FD_SET(buf->fd, &fds);

	/* The select(2) interface is silly */
	return select(buf->fd + 1, &fds, NULL, NULL, &tv);
}

int gitno_extract_host_and_port(char **host, char **port, const char *url, const char *default_port)
{
	char *colon, *slash, *delim;
	int error = GIT_SUCCESS;

	colon = strchr(url, ':');
	slash = strchr(url, '/');

	if (slash == NULL)
			return git__throw(GIT_EOBJCORRUPTED, "Malformed URL: missing /");

	if (colon == NULL) {
		*port = git__strdup(default_port);
	} else {
		*port = git__strndup(colon + 1, slash - colon - 1);
	}
	if (*port == NULL)
		return GIT_ENOMEM;;


	delim = colon == NULL ? slash : colon;
	*host = git__strndup(url, delim - url);
	if (*host == NULL) {
		git__free(*port);
		error = GIT_ENOMEM;
	}

	return error;
}
