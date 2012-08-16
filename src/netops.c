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
#       include <arpa/inet.h>
#else
#	include <ws2tcpip.h>
#	ifdef _MSC_VER
#		pragma comment(lib, "ws2_32.lib")
#	endif
#endif

#ifdef GIT_SSL
# include <openssl/ssl.h>
# include <openssl/x509v3.h>
#endif

#include <ctype.h>
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

	giterr_set(GITERR_NET, "%s: %s", str, err_str);
	LocalFree(err_str);
}
#else
static void net_set_error(const char *str)
{
	giterr_set(GITERR_NET, "%s: %s", str, strerror(errno));
}
#endif

#ifdef GIT_SSL
static int ssl_set_error(gitno_ssl *ssl, int error)
{
	int err;
	unsigned long e;

	err = SSL_get_error(ssl->ssl, error);

	assert(err != SSL_ERROR_WANT_READ);
	assert(err != SSL_ERROR_WANT_WRITE);

	switch (err) {
	case SSL_ERROR_WANT_CONNECT:
	case SSL_ERROR_WANT_ACCEPT:
		giterr_set(GITERR_NET, "SSL error: connection failure\n");
		break;
	case SSL_ERROR_WANT_X509_LOOKUP:
		giterr_set(GITERR_NET, "SSL error: x509 error\n");
		break;
	case SSL_ERROR_SYSCALL:
		e = ERR_get_error();
		if (e > 0) {
			giterr_set(GITERR_NET, "SSL error: %s",
					ERR_error_string(e, NULL));
			break;
		} else if (error < 0) {
			giterr_set(GITERR_OS, "SSL error: syscall failure");
			break;
		}
		giterr_set(GITERR_NET, "SSL error: received early EOF");
		break;
	case SSL_ERROR_SSL:
		e = ERR_get_error();
		giterr_set(GITERR_NET, "SSL error: %s",
				ERR_error_string(e, NULL));
		break;
	case SSL_ERROR_NONE:
	case SSL_ERROR_ZERO_RETURN:
	default:
		giterr_set(GITERR_NET, "SSL error: unknown error");
		break;
	}
	return -1;
}
#endif

int gitno_recv(gitno_buffer *buf)
{
	return buf->recv(buf);
}

#ifdef GIT_SSL
static int gitno__recv_ssl(gitno_buffer *buf)
{
	int ret;

	do {
		ret = SSL_read(buf->ssl->ssl, buf->data + buf->offset, buf->len - buf->offset);
	} while (SSL_get_error(buf->ssl->ssl, ret) == SSL_ERROR_WANT_READ);

	if (ret < 0) {
		net_set_error("Error receiving socket data");
		return -1;
	}

	buf->offset += ret;
	return ret;
}
#endif

int gitno__recv(gitno_buffer *buf)
{
	int ret;

	ret = p_recv(buf->fd, buf->data + buf->offset, buf->len - buf->offset, 0);
	if (ret < 0) {
		net_set_error("Error receiving socket data");
		return -1;
	}

	buf->offset += ret;
	return ret;
}

void gitno_buffer_setup_callback(
	git_transport *t,
	gitno_buffer *buf,
	char *data,
	size_t len,
	int (*recv)(gitno_buffer *buf), void *cb_data)
{
	memset(buf, 0x0, sizeof(gitno_buffer));
	memset(data, 0x0, len);
	buf->data = data;
	buf->len = len;
	buf->offset = 0;
	buf->fd = t->socket;
	buf->recv = recv;
	buf->cb_data = cb_data;
}

void gitno_buffer_setup(git_transport *t, gitno_buffer *buf, char *data, size_t len)
{
#ifdef GIT_SSL
	if (t->use_ssl) {
		gitno_buffer_setup_callback(t, buf, data, len, gitno__recv_ssl, NULL);
		buf->ssl = &t->ssl;
	} else
#endif
		gitno_buffer_setup_callback(t, buf, data, len, gitno__recv, NULL);
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
#ifdef GIT_SSL
	int ret;
#endif

	if (!t->use_ssl)
		return 0;

#ifdef GIT_SSL

	do {
		ret = SSL_shutdown(t->ssl.ssl);
	} while (ret == 0);
	if (ret < 0)
		return ssl_set_error(&t->ssl, ret);

	SSL_free(t->ssl.ssl);
	SSL_CTX_free(t->ssl.ctx);
#endif
	return 0;
}


#ifdef GIT_SSL
/* Match host names according to RFC 2818 rules */
static int match_host(const char *pattern, const char *host)
{
	for (;;) {
		char c = tolower(*pattern++);

		if (c == '\0')
			return *host ? -1 : 0;

		if (c == '*') {
			c = *pattern;
			/* '*' at the end matches everything left */
			if (c == '\0')
				return 0;

	/*
	 * We've found a pattern, so move towards the next matching
	 * char. The '.' is handled specially because wildcards aren't
	 * allowed to cross subdomains.
	 */

			while(*host) {
				char h = tolower(*host);
				if (c == h)
					return match_host(pattern, host++);
				if (h == '.')
					return match_host(pattern, host);
				host++;
			}
			return -1;
		}

		if (c != tolower(*host++))
			return -1;
	}

	return -1;
}

static int check_host_name(const char *name, const char *host)
{
	if (!strcasecmp(name, host))
		return 0;

	if (match_host(name, host) < 0)
		return -1;

	return 0;
}

static int verify_server_cert(git_transport *t, const char *host)
{
	X509 *cert;
	X509_NAME *peer_name;
	ASN1_STRING *str;
	unsigned char *peer_cn = NULL;
	int matched = -1, type = GEN_DNS;
	GENERAL_NAMES *alts;
	struct in6_addr addr6;
	struct in_addr addr4;
	void *addr;
	int i = -1,j;

	if (SSL_get_verify_result(t->ssl.ssl) != X509_V_OK) {
		giterr_set(GITERR_SSL, "The SSL certificate is invalid");
		return -1;
	}

	/* Try to parse the host as an IP address to see if it is */
	if (inet_pton(AF_INET, host, &addr4)) {
		type = GEN_IPADD;
		addr = &addr4;
	} else {
		if(inet_pton(AF_INET6, host, &addr6)) {
			type = GEN_IPADD;
			addr = &addr6;
		}
	}


	cert = SSL_get_peer_certificate(t->ssl.ssl);

	/* Check the alternative names */
	alts = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (alts) {
		int num;

		num = sk_GENERAL_NAME_num(alts);
		for (i = 0; i < num && matched != 1; i++) {
			const GENERAL_NAME *gn = sk_GENERAL_NAME_value(alts, i);
			const char *name = (char *) ASN1_STRING_data(gn->d.ia5);
			size_t namelen = (size_t) ASN1_STRING_length(gn->d.ia5);

			/* Skip any names of a type we're not looking for */
			if (gn->type != type)
				continue;

			if (type == GEN_DNS) {
				/* If it contains embedded NULs, don't even try */
				if (memchr(name, '\0', namelen))
					continue;

				if (check_host_name(name, host) < 0)
					matched = 0;
				else
					matched = 1;
			} else if (type == GEN_IPADD) {
				/* Here name isn't so much a name but a binary representation of the IP */
				matched = !!memcmp(name, addr, namelen);
			}
		}
	}
	GENERAL_NAMES_free(alts);

	if (matched == 0)
		goto cert_fail;

	if (matched == 1)
		return 0;

	/* If no alternative names are available, check the common name */
	peer_name = X509_get_subject_name(cert);
	if (peer_name == NULL)
		goto on_error;

	if (peer_name) {
		/* Get the index of the last CN entry */
		while ((j = X509_NAME_get_index_by_NID(peer_name, NID_commonName, i)) >= 0)
			i = j;
	}

	if (i < 0)
		goto on_error;

	str = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(peer_name, i));
	if (str == NULL)
		goto on_error;

	/* Work around a bug in OpenSSL whereby ASN1_STRING_to_UTF8 fails if it's already in utf-8 */
	if (ASN1_STRING_type(str) == V_ASN1_UTF8STRING) {
		int size = ASN1_STRING_length(str);

		if (size > 0) {
			peer_cn = OPENSSL_malloc(size + 1);
			GITERR_CHECK_ALLOC(peer_cn);
			memcpy(peer_cn, ASN1_STRING_data(str), size);
			peer_cn[size] = '\0';
		}
	} else {
		int size = ASN1_STRING_to_UTF8(&peer_cn, str);
		GITERR_CHECK_ALLOC(peer_cn);
		if (memchr(peer_cn, '\0', size))
			goto cert_fail;
	}

	if (check_host_name((char *)peer_cn, host) < 0)
		goto cert_fail;

	OPENSSL_free(peer_cn);

	return 0;

on_error:
	OPENSSL_free(peer_cn);
	return ssl_set_error(&t->ssl, 0);

cert_fail:
	OPENSSL_free(peer_cn);
	giterr_set(GITERR_SSL, "Certificate host name check failed");
	return -1;
}

static int ssl_setup(git_transport *t, const char *host)
{
	int ret;

	SSL_library_init();
	SSL_load_error_strings();
	t->ssl.ctx = SSL_CTX_new(SSLv23_method());
	if (t->ssl.ctx == NULL)
		return ssl_set_error(&t->ssl, 0);

	SSL_CTX_set_mode(t->ssl.ctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_verify(t->ssl.ctx, SSL_VERIFY_NONE, NULL);
	if (!SSL_CTX_set_default_verify_paths(t->ssl.ctx))
		return ssl_set_error(&t->ssl, 0);

	t->ssl.ssl = SSL_new(t->ssl.ctx);
	if (t->ssl.ssl == NULL)
		return ssl_set_error(&t->ssl, 0);

	if((ret = SSL_set_fd(t->ssl.ssl, t->socket)) == 0)
		return ssl_set_error(&t->ssl, ret);

	if ((ret = SSL_connect(t->ssl.ssl)) <= 0)
		return ssl_set_error(&t->ssl, ret);

	if (t->check_cert && verify_server_cert(t, host) < 0)
		return -1;

	return 0;
}
#else
static int ssl_setup(git_transport *t, const char *host)
{
	GIT_UNUSED(t);
	GIT_UNUSED(host);
	return 0;
}
#endif

int gitno_connect(git_transport *t, const char *host, const char *port)
{
	struct addrinfo *info = NULL, *p;
	struct addrinfo hints;
	int ret;
	GIT_SOCKET s = INVALID_SOCKET;

	memset(&hints, 0x0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;

	if ((ret = p_getaddrinfo(host, port, &hints, &info)) < 0) {
		giterr_set(GITERR_NET,
			"Failed to resolve address for %s: %s", host, p_gai_strerror(ret));
		return -1;
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
	if (s == INVALID_SOCKET && p == NULL) {
		giterr_set(GITERR_OS, "Failed to connect to %s", host);
		return -1;
	}

	t->socket = s;
	p_freeaddrinfo(info);

	if (t->use_ssl && ssl_setup(t, host) < 0)
		return -1;

	return 0;
}

#ifdef GIT_SSL
static int send_ssl(gitno_ssl *ssl, const char *msg, size_t len)
{
	int ret;
	size_t off = 0;

	while (off < len) {
		ret = SSL_write(ssl->ssl, msg + off, len - off);
		if (ret <= 0 && ret != SSL_ERROR_WANT_WRITE)
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
	if (t->use_ssl)
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
	char *colon, *slash, *delim, *at;

	colon = strchr(url, ':');
	slash = strchr(url, '/');
	at = strchr(url, '@');

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
	url = at ? at + 1 : url;
	*host = git__strndup(url, delim - url);
	GITERR_CHECK_ALLOC(*host);

	return 0;
}
