/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "streams/socket.h"

#include "posix.h"
#include "netops.h"
#include "remote.h"
#include "registry.h"
#include "stream.h"

#ifndef _WIN32
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <sys/select.h>
#	include <sys/time.h>
#	include <netdb.h>
#	include <netinet/in.h>
#       include <arpa/inet.h>
#       include <unistd.h>
#       include <fcntl.h>
#else
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	ifdef _MSC_VER
#		pragma comment(lib, "ws2_32")
#	endif
#endif

#define GIT_STREAM_CONNECT_TIMEOUT	60
#define GIT_STREAM_BUFSIZ		2048U

static int socket_connect_event(git_remote *remote, void *cbref, git_event_t events);

#ifdef GIT_WIN32
static void net_set_error(const char *str)
{
	int error = WSAGetLastError();
	char * win32_error = git_win32_get_error_message(error);

	if (win32_error) {
		git_error_set(GIT_ERROR_NET, "%s: %s", str, win32_error);
		git__free(win32_error);
	} else {
		git_error_set(GIT_ERROR_NET, "%s", str);
	}
}
#else
static void net_set_error(const char *str)
{
	git_error_set(GIT_ERROR_NET, "%s: %s", str, strerror(errno));
}
#endif

static int close_socket(git_socket s)
{
	if (s == INVALID_SOCKET)
		return 0;

#ifdef GIT_WIN32
	if (SOCKET_ERROR == closesocket(s))
		return -1;

	if (0 != WSACleanup()) {
		git_error_set(GIT_ERROR_OS, "winsock cleanup failed");
		return -1;
	}

	return 0;
#else
	return close(s);
#endif

}

static int socket_connect_failed(git_socket_stream *st, int err)
{
	int syserr;

	/* Oops, we couldn't connect to any address */
	p_freeaddrinfo(st->info);
	
	if((syserr = close_socket(st->s)) < 0)
		err = syserr;
	
	st->s = INVALID_SOCKET;

	return err;
}

static int socket_connect_failed_err(git_socket_stream *st)
{
	git_error_set(GIT_ERROR_NET, "failed to connect to %s", st->host);

	return socket_connect_failed(st, GIT_ERROR);
}

static int socket_connect_connected(git_socket_stream *st)
{
	p_freeaddrinfo(st->info);
	
	return GIT_OK;
}

static int socket_connect_next(git_socket_stream *st)
{
	git_remote *remote = st->remote;
	struct addrinfo *curinfo;
	git_socket s;
	int err;
	
	for(curinfo = st->curinfo; curinfo; curinfo = curinfo->ai_next)
	{
		s = socket(curinfo->ai_family, curinfo->ai_socktype | SOCK_CLOEXEC, curinfo->ai_protocol);

		if (s == INVALID_SOCKET)
			continue;
		
		if(remote)
		{
			if(p_setfd_nonblocking(s) < 0)
			{
				if((err = close_socket(s)) < 0)
					return socket_connect_failed(st, err);
				else
					continue;
			}
		}

		st->s = s;

#ifdef GIT_WIN32
		if(connect(s, curinfo->ai_addr, (socklen_t) curinfo->ai_addrlen) == SOCKET_ERROR)
#else
		if(connect(s, curinfo->ai_addr, (socklen_t) curinfo->ai_addrlen) < 0)
#endif
		{
#ifdef GIT_WIN32
			if(WSAGetLastError() == WSAEWOULDBLOCK)
#else
			if(errno == EINPROGRESS)
#endif
			{
				st->curinfo = curinfo->ai_next;
				
				if((err = git_remote_add_performcb(remote, socket_connect_event, st)) < 0)
					return socket_connect_failed(st, err);

				err = remote->callbacks.set_fd_events(s, GIT_EVENT_WRITE, GIT_STREAM_CONNECT_TIMEOUT, remote->cbref);

				if(err < 0)
					return socket_connect_failed(st, err);
				else
					return GIT_EAGAIN;
			}
		}
		else
			return socket_connect_connected(st);

		/* If we can't connect, try the next one */
		if((err = close_socket(s)) < 0)
			return socket_connect_failed(st, err);
	}
	
	/* Oops, we couldn't connect to any address */
	return socket_connect_failed_err(st);
}

int socket_connect_event(git_remote *remote, void *cbref, git_event_t events)
{
	git_socket_stream *st = cbref;
	int uerr, err;

	uerr = remote->callbacks.set_fd_events(st->s, GIT_EVENT_NONE, 0, remote->cbref);

	if((events & GIT_EVENT_ERR) || uerr < 0)
	{
		if((err = close_socket(st->s)) < 0)
			return socket_connect_failed(st, err);
		
		if(uerr < 0)
			return socket_connect_failed(st, uerr);
		else
			return socket_connect_next(st);
	}

	if((events & GIT_EVENT_WRITE))
		return socket_connect_connected(st);
	else
	{
		if((events & GIT_EVENT_TIMEOUT))
		{
			if((err = close_socket(st->s)) < 0)
				return socket_connect_failed(st, err);
			else
				return socket_connect_next(st);
		}
		else
		{
			if((err = git_remote_add_performcb(remote, socket_connect_event, st)))
				return socket_connect_failed(st, err);
			else
				return GIT_EAGAIN;
		}
	}
}

static int socket_connect(git_stream *stream)
{
	struct addrinfo hints;
	git_socket_stream *st = (git_socket_stream *) stream;
	int ret;

#ifdef GIT_WIN32
	/* on win32, the WSA context needs to be initialized
	 * before any socket calls can be performed */
	WSADATA wsd;

	if (WSAStartup(MAKEWORD(2,2), &wsd) != 0) {
		git_error_set(GIT_ERROR_OS, "winsock init failed");
		return -1;
	}

	if (LOBYTE(wsd.wVersion) != 2 || HIBYTE(wsd.wVersion) != 2) {
		WSACleanup();
		git_error_set(GIT_ERROR_OS, "winsock init failed");
		return -1;
	}
#endif

	memset(&hints, 0x0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;

	if ((ret = p_getaddrinfo(st->host, st->port, &hints, &st->info)) != 0) {
		git_error_set(GIT_ERROR_NET,
			   "failed to resolve address for %s: %s", st->host, p_gai_strerror(ret));
		return -1;
	}
	
	st->curinfo = st->info;

	return socket_connect_next(st);
}

static ssize_t socket_write(git_stream *stream, const char *data, size_t len, int flags)
{
	git_socket_stream *st = (git_socket_stream *) stream;
	ssize_t written;

	errno = 0;

	if ((written = p_send(st->s, data, len, flags)) < 0) {
		net_set_error("error sending data");
		return -1;
	}

	return written;
}

static ssize_t socket_read(git_stream *stream, void *data, size_t len)
{
	ssize_t ret;
	git_socket_stream *st = (git_socket_stream *) stream;

	if ((ret = p_recv(st->s, data, len, 0)) < 0)
		net_set_error("error receiving socket data");

	return ret;
}

static int socket_close(git_stream *stream)
{
	git_remote *remote;
	git_socket_stream *st = (git_socket_stream *) stream;
	int error;

	if((remote = st->remote) && remote->callbacks.set_fd_events)
		remote->callbacks.set_fd_events(st->s, GIT_EVENT_NONE, 0U, remote->cbref);

	error = close_socket(st->s);
	st->s = INVALID_SOCKET;

	return error;
}

static void socket_free(git_stream *stream)
{
	git_socket_stream *st = (git_socket_stream *) stream;

	git_buf_dispose(&st->sock_buf);

	git__free(st->host);
	git__free(st->port);
	git__free(st);
}

static int default_socket_stream_new(
	git_stream **out,
	git_remote *remote,
	const char *host,
	const char *port)
{
	git_socket_stream *st;

	assert(out && host && port);

	st = git__calloc(1, sizeof(git_socket_stream));
	GIT_ERROR_CHECK_ALLOC(st);

	st->host = git__strdup(host);
	GIT_ERROR_CHECK_ALLOC(st->host);

	if (port) {
		st->port = git__strdup(port);
		GIT_ERROR_CHECK_ALLOC(st->port);
	}

	st->parent.version = GIT_STREAM_VERSION;
	st->parent.connect = socket_connect;
	st->parent.write = socket_write;
	st->parent.read = socket_read;
	st->parent.close = socket_close;
	st->parent.free = socket_free;
	st->s = INVALID_SOCKET;
	
	git_buf_init(&st->sock_buf, GIT_STREAM_BUFSIZ);
	st->remote = remote;

	*out = (git_stream *) st;
	return 0;
}

int git_socket_stream_new(
	git_stream **out,
	git_remote *remote,
	const char *host,
	const char *port)
{
	int (*init)(git_stream **, git_remote *, const char *, const char *) = NULL;
	git_stream_registration custom = {0};
	int error;

	assert(out && host && port);

	if ((error = git_stream_registry_lookup(&custom, GIT_STREAM_STANDARD)) == 0)
		init = custom.init;
	else if (error == GIT_ENOTFOUND)
		init = default_socket_stream_new;
	else
		return error;

	if (!init) {
		git_error_set(GIT_ERROR_NET, "there is no socket stream available");
		return -1;
	}

	return init(out, remote, host, port);
}
