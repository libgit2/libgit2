/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifdef GIT_GNUTLS

#include <gnutls/gnutls.h>

#include "stream.h"
#include "socket_stream.h"
#include "git2/transport.h"

void set_gnutls_error(int error)
{
	giterr_set(GITERR_SSL, "gnutls: %s", gnutls_strerror(error));
}

typedef struct {
	git_stream parent;
	git_socket_stream *socket;
	gnutls_session_t session;
	git_cert_x509 cert_info;
} gnutls_stream;

static int verify_server_cert(gnutls_session_t session, const char *host)
{
	int error;
	unsigned int status = 0;

	if ((error = gnutls_certificate_verify_peers3(session, host, &status)) < 0) {
		set_gnutls_error(error);
		return -1;
	}

	if (!status)
		return 0;
	else
		return GIT_ECERTIFICATE;
}

static ssize_t gnutls_stream_write(git_stream *stream, void *data, size_t len, int flags)
{
	ssize_t ret;
	size_t off;
	gnutls_stream *st = (gnutls_stream *) stream;

	GIT_UNUSED(flags);

	while (off < len) {
		ret = gnutls_record_send(st->session, data + off, len - off);
		if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN)
			continue; /* try again with the same params */

		if (ret < 0) {
			set_gnutls_error(ret);
			return -1;
		}

		off += ret;
	}

	return ret;
}

static ssize_t gnutls_stream_read(git_stream *stream, void *data, size_t len)
{
	ssize_t ret;
	gnutls_stream *st = (gnutls_stream *) stream;

	do {
		ret = gnutls_record_recv(st->session, data, len);
	} while (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN);

	if (ret < 0) {
		set_gnutls_error(ret);
		return -1;
	}

	return ret;
}

static int gnutls_connect(git_stream *stream)
{
	int error;
	gnutls_stream *st = (gnutls_stream *) stream;

	if ((error = git_stream_connect((git_stream *)st->socket)) < 0)
		return error;

	/* ideally we'd have functions that talk to the socket, but for now */
	gnutls_transport_set_int(st->session, st->socket->s);

	do {
		error = gnutls_handshake(st->session);
	} while (!gnutls_error_is_fatal(error));

	if (error < 0) {
		set_gnutls_error(error);
		return -1;
	}

	return verify_server_cert(st->session, st->socket->host);
}

static int gnutls_stream_close(git_stream *stream)
{
	gnutls_stream *st = (gnutls_stream *) stream;
	int error;

	do {
		error = gnutls_bye(st->session, GNUTLS_SHUT_RDWR);
	} while (error == GNUTLS_E_INTERRUPTED || error == GNUTLS_E_AGAIN);

	return git_stream_close((git_stream *)st->socket);
}

static void gnutls_stream_free(git_stream *stream)
{
	gnutls_stream *st = (gnutls_stream *) stream;

	gnutls_deinit(st->session);
	git__free(st);
}

int git_gnutls_stream_new(git_stream **out, const char *host, const char *port)
{
	gnutls_stream *st;
	int error;

	st = git__calloc(1, sizeof(gnutls_stream));
	GITERR_CHECK_ALLOC(st);

	if (git_socket_stream_new((git_stream **) &st->socket, host, port))
		return -1;

	if ((error = gnutls_init(&st->session, GNUTLS_CLIENT)) < 0) {
		git_stream_free((git_stream *) st->socket);
		git__free(st);
		return -1;
	}

	st->parent.encrypted = 1;
	st->parent.connect = gnutls_connect;
	st->parent.certificate = gnutls_certificate;
	st->parent.read = gnutls_stream_read;
	st->parent.write = gnutls_stream_write;
	st->parent.close = gnutls_stream_close;
	st->parent.free = gnutls_stream_free;

	*out = (git_stream *) st;
	return 0;
}

#else

int git_gnutls_stream_new(git_stream **out, const char *host, const char *port)
{
	giterr_set(GITERR_SSL, "GnuTLS is not supported in this version");
	return -1;
}

#endif
