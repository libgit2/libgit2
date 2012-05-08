/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef _WIN32
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <sys/select.h>
#	include <sys/time.h>
#	include <netdb.h>
#else
#	define _WIN32_WINNT 0x0501
#	include <winsock2.h>
#	include <Ws2tcpip.h>
#	ifdef _MSC_VER
#		pragma comment(lib, "Ws2_32.lib")
#	endif
#endif

#ifdef GIT_GNUTLS
# include <gnutls/openssl.h>
# include <gnutls/gnutls.h>
# include <gnutls/x509.h>
#endif

#include "git2/errors.h"

#include "common.h"
#include "netops.h"
#include "posix.h"
#include "buffer.h"
#include "transport.h"

#ifdef GIT_WIN32
static void net_set_error(const char *str)
{
	int size, error = WSAGetLastError();
	LPSTR err_str = NULL;

	size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			     0, error, 0, (LPSTR)&err_str, 0, 0);

	giterr_set(GITERR_NET, "%s: $s", str, err_str);
	LocalFree(err_str);
}
#else
static void net_set_error(const char *str)
{
	giterr_set(GITERR_NET, "%s: %s", str, strerror(errno));
}
#endif

#ifdef GIT_GNUTLS
static int ssl_set_error(int error)
{
	giterr_set(GITERR_NET, "SSL error: (%s) %s", gnutls_strerror_name(error), gnutls_strerror(error));
	return -1;
}
#elif GIT_OPENSSL
static int ssl_set_error(gitno_ssl *ssl, int error)
{
	int err;
	err = SSL_get_error(ssl->ssl, error);
	giterr_set(GITERR_NET, "SSL error: %s", ERR_error_string(err, NULL));
	return -1;
}
#endif

void gitno_buffer_setup(git_transport *t, gitno_buffer *buf, char *data, unsigned int len)
{
	memset(buf, 0x0, sizeof(gitno_buffer));
	memset(data, 0x0, len);
	buf->data = data;
	buf->len = len;
	buf->offset = 0;
	buf->fd = t->socket;
#ifdef GIT_SSL
	if (t->encrypt)
		buf->ssl = &t->ssl;
#endif
}

#ifdef GIT_GNUTLS
static int ssl_recv(gitno_ssl *ssl, void *data, size_t len)
{
	int ret;

	do {
		ret = gnutls_record_recv(ssl->session, data, len);
	} while(ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN);

	if (ret < 0) {
		ssl_set_error(ret);
		return -1;
	}

	return ret;
}
#elif defined(GIT_OPENSSL)
static int ssl_recv(gitno_ssl *ssl, void *data, size_t len)
{
	int ret;

	do {
		ret = SSL_read(ssl->ssl, data, len);
	} while (SSL_get_error(ssl->ssl, ret) == SSL_ERROR_WANT_READ);

	if (ret < 0)
		return ssl_set_error(ssl, ret);

	return ret;
}
#endif

int gitno_recv(gitno_buffer *buf)
{
	int ret;

#ifdef GIT_SSL
	if (buf->ssl != NULL) {
		if ((ret = ssl_recv(buf->ssl, buf->data + buf->offset, buf->len - buf->offset)) < 0)
			return -1;
	} else {
		ret = p_recv(buf->fd, buf->data + buf->offset, buf->len - buf->offset, 0);
		if (ret < 0) {
			net_set_error("Error receiving socket data");
			return -1;
		}
	}
#else
	ret = p_recv(buf->fd, buf->data + buf->offset, buf->len - buf->offset, 0);
	if (ret < 0) {
		net_set_error("Error receiving socket data");
		return -1;
	}
#endif

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

int gitno_ssl_teardown(git_transport *t)
{
	if (!t->encrypt)
		return 0;

#ifdef GIT_GNUTLS
	gnutls_deinit(t->ssl.session);
	gnutls_certificate_free_credentials(t->ssl.cred);
	gnutls_global_deinit();
#elif defined(GIT_OPENSS)
	int ret;

	do {
		ret = SSL_shutdown(t->ssl.ssl);
	} while (ret == 0);
	if (ret < 0)
		return ssl_set_error(&t->ssl);

	SSL_free(t->ssl.ssl);
	SSL_CTX_free(t->ssl.ctx);
	SSL_free_error_strings();
#endif
	return 0;
}


static int ssl_setup(git_transport *t)
{
#ifdef GIT_GNUTLS
	int ret;

	if ((ret = gnutls_global_init()) < 0)
		return ssl_set_error(ret);

	if ((ret = gnutls_certificate_allocate_credentials(&t->ssl.cred)) < 0)
		return ssl_set_error(ret);

	gnutls_init(&t->ssl.session, GNUTLS_CLIENT);
	//gnutls_certificate_set_verify_function(ssl->cred, SSL_VERIFY_NONE);
	gnutls_credentials_set(t->ssl.session, GNUTLS_CRD_CERTIFICATE, t->ssl.cred);

	if ((ret = gnutls_priority_set_direct (t->ssl.session, "NORMAL", NULL)) < 0)
		return ssl_set_error(ret);

	gnutls_transport_set_ptr(t->ssl.session, (gnutls_transport_ptr_t) t->socket);

	do {
		ret = gnutls_handshake(t->ssl.session);
	} while (ret < 0 && !gnutls_error_is_fatal(ret));

	if (ret < 0) {
		ssl_set_error(ret);
		goto on_error;
	}

	return 0;

on_error:
	gnutls_deinit(t->ssl.session);
	gnutls_global_deinit();
	return -1;
#elif defined(GIT_OPENSSL)
	int ret;

	SSL_library_init();
	SSL_load_error_strings();
	t->ssl.ctx = SSL_CTX_new(SSLv23_method());
	if (t->ssl.ctx == NULL)
		return ssl_set_error(&t->ssl, 0);

	SSL_CTX_set_mode(t->ssl.ctx, SSL_MODE_AUTO_RETRY);

	t->ssl.ssl = SSL_new(t->ssl.ctx);
	if (t->ssl.ssl == NULL)
		return ssl_set_error(&t->ssl, 0);

	if((ret = SSL_set_fd(t->ssl.ssl, t->socket)) == 0)
		return ssl_set_error(&t->ssl, ret);

	if ((ret = SSL_connect(t->ssl.ssl)) <= 0)
		return ssl_set_error(&t->ssl, ret);

	return 0;
#else
	GIT_UNUSED(t);
	return 0;
#endif
}
int gitno_connect(git_transport *t, const char *host, const char *port)
{
	struct addrinfo *info = NULL, *p;
	struct addrinfo hints;
	int ret;
	GIT_SOCKET s = INVALID_SOCKET;

	memset(&hints, 0x0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((ret = getaddrinfo(host, port, &hints, &info)) < 0) {
		giterr_set(GITERR_NET, "Failed to resolve address for %s: %s", host, gai_strerror(ret));
		return INVALID_SOCKET;
	}

	for (p = info; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (s == INVALID_SOCKET) {
			net_set_error("error creating socket");
			break;
		}

		if (connect(s, p->ai_addr, (socklen_t)p->ai_addrlen) == 0)
			break;

		/* If we can't connect, try the next one */
		gitno_close(s);
		s = INVALID_SOCKET;
	}

	/* Oops, we couldn't connect to any address */
	if (s == INVALID_SOCKET && p == NULL)
		giterr_set(GITERR_OS, "Failed to connect to %s", host);

	t->socket = s;
	freeaddrinfo(info);

	if (t->encrypt && ssl_setup(t) < 0)
		return -1;

	return 0;
}

#ifdef GIT_GNUTLS
static int send_ssl(gitno_ssl *ssl, const char *msg, size_t len)
{
	int ret;
	size_t off = 0;

	while (off < len) {
		ret = gnutls_record_send(ssl->session, msg + off, len - off);
		if (ret < 0) {
			if (gnutls_error_is_fatal(ret))
				return ssl_set_error(ret);

			ret = 0;
		}
		off += ret;
	}

	return off;
}
#elif defined(GIT_OPENSSL)
static int send_ssl(gitno_ssl *ssl, const char *msg, size_t len)
{
	int ret;
	size_t off = 0;

	while (off < len) {
		ret = SSL_write(ssl->ssl, msg + off, len - off);
		if (ret <= 0)
			return ssl_set_error(ssl, ret);

		off += ret;
	}

	return off;
}
#endif

int gitno_send(git_transport *t, const char *msg, size_t len, int flags)
{
	int ret;
	size_t off = 0;

#ifdef GIT_SSL
	if (t->encrypt)
		return send_ssl(&t->ssl, msg, len);
#endif

	while (off < len) {
		errno = 0;
		ret = p_send(t->socket, msg + off, len - off, flags);
		if (ret < 0) {
			net_set_error("Error sending data");
			return -1;
		}

		off += ret;
	}

	return (int)off;
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
	return select((int)buf->fd + 1, &fds, NULL, NULL, &tv);
}

int gitno_extract_host_and_port(char **host, char **port, const char *url, const char *default_port)
{
	char *colon, *slash, *delim;

	colon = strchr(url, ':');
	slash = strchr(url, '/');

	if (slash == NULL) {
		giterr_set(GITERR_NET, "Malformed URL: missing /");
		return -1;
	}

	if (colon == NULL) {
		*port = git__strdup(default_port);
	} else {
		*port = git__strndup(colon + 1, slash - colon - 1);
	}
	GITERR_CHECK_ALLOC(*port);

	delim = colon == NULL ? slash : colon;
	*host = git__strndup(url, delim - url);
	GITERR_CHECK_ALLOC(*host);

	return 0;
}
