/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/net.h"
#include "git2/common.h"
#include "git2/types.h"
#include "git2/errors.h"
#include "git2/net.h"
#include "git2/revwalk.h"

#include "vector.h"
#include "transport.h"
#include "pkt.h"
#include "common.h"
#include "netops.h"
#include "filebuf.h"
#include "repository.h"
#include "fetch.h"
#include "protocol.h"

typedef struct {
	git_transport parent;
	char buff[65536];
#ifdef GIT_WIN32
	WSADATA wsd;
#endif
} transport_git;

/*
 * Create a git procol request.
 *
 * For example: 0035git-upload-pack /libgit2/libgit2\0host=github.com\0
 */
static int gen_proto(git_buf *request, const char *cmd, const char *url)
{
	char *delim, *repo;
	char default_command[] = "git-upload-pack";
	char host[] = "host=";
	size_t len;

	delim = strchr(url, '/');
	if (delim == NULL) {
		giterr_set(GITERR_NET, "Malformed URL");
		return -1;
	}

	repo = delim;

	delim = strchr(url, ':');
	if (delim == NULL)
		delim = strchr(url, '/');

	if (cmd == NULL)
		cmd = default_command;

	len = 4 + strlen(cmd) + 1 + strlen(repo) + 1 + strlen(host) + (delim - url) + 1;

	git_buf_grow(request, len);
	git_buf_printf(request, "%04x%s %s%c%s",
		(unsigned int)(len & 0x0FFFF), cmd, repo, 0, host);
	git_buf_put(request, url, delim - url);
	git_buf_putc(request, '\0');

	if (git_buf_oom(request))
		return -1;

	return 0;
}

static int send_request(git_transport *t, const char *cmd, const char *url)
{
	int error;
	git_buf request = GIT_BUF_INIT;

	error = gen_proto(&request, cmd, url);
	if (error < 0)
		goto cleanup;

	error = gitno_send(t, request.ptr, request.size, 0);

cleanup:
	git_buf_free(&request);
	return error;
}

/*
 * Parse the URL and connect to a server, storing the socket in
 * out. For convenience this also takes care of asking for the remote
 * refs
 */
static int do_connect(transport_git *t, const char *url)
{
	char *host, *port;
	const char prefix[] = "git://";

	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	if (gitno_extract_host_and_port(&host, &port, url, GIT_DEFAULT_PORT) < 0)
		return -1;

	if (gitno_connect((git_transport *)t, host, port) < 0)
		goto on_error;

	if (send_request((git_transport *)t,
			 t->parent.direction ? "git-receive-pack" : NULL, url) < 0)
		goto on_error;

	git__free(host);
	git__free(port);

	return 0;

on_error:
	git__free(host);
	git__free(port);
	gitno_close(t->parent.socket);
	return -1;
}

/*
 * Since this is a network connection, we need to parse and store the
 * pkt-lines at this stage and keep them there.
 */
static int git_connect(git_transport *transport, int direction)
{
	transport_git *t = (transport_git *) transport;

	t->parent.direction = direction;

	/* Connect and ask for the refs */
	if (do_connect(t, transport->url) < 0)
		return -1;

	gitno_buffer_setup(transport, &transport->buffer, t->buff, sizeof(t->buff));

	t->parent.connected = 1;
	if (git_protocol_store_refs(transport, 1) < 0)
		return -1;

	if (git_protocol_detect_caps(git_vector_get(&transport->refs, 0), &transport->caps) < 0)
		return -1;

	return 0;
}

static int git_negotiation_step(struct git_transport *transport, void *data, size_t len)
{
	return gitno_send(transport, data, len, 0);
}

static int git_close(git_transport *t)
{
	git_buf buf = GIT_BUF_INIT;

	if (git_pkt_buffer_flush(&buf) < 0)
		return -1;
	/* Can't do anything if there's an error, so don't bother checking  */
	gitno_send(t, buf.ptr, buf.size, 0);
	git_buf_free(&buf);

	if (gitno_close(t->socket) < 0) {
		giterr_set(GITERR_NET, "Failed to close socket");
		return -1;
	}

	t->connected = 0;

#ifdef GIT_WIN32
	WSACleanup();
#endif

	return 0;
}

static void git_free(git_transport *transport)
{
	transport_git *t = (transport_git *) transport;
	git_vector *refs = &transport->refs;
	unsigned int i;

	for (i = 0; i < refs->length; ++i) {
		git_pkt *p = git_vector_get(refs, i);
		git_pkt_free(p);
	}
	git_vector_free(refs);

	refs = &transport->common;
	for (i = 0; i < refs->length; ++i) {
		git_pkt *p = git_vector_get(refs, i);
		git_pkt_free(p);
	}
	git_vector_free(refs);

	git__free(t->parent.url);
	git__free(t);
}

int git_transport_git(git_transport **out)
{
	transport_git *t;
#ifdef GIT_WIN32
	int ret;
#endif

	t = git__malloc(sizeof(transport_git));
	GITERR_CHECK_ALLOC(t);

	memset(t, 0x0, sizeof(transport_git));
	if (git_vector_init(&t->parent.common, 8, NULL))
		goto on_error;

	if (git_vector_init(&t->parent.refs, 16, NULL) < 0)
		goto on_error;

	t->parent.connect = git_connect;
	t->parent.negotiation_step = git_negotiation_step;
	t->parent.close = git_close;
	t->parent.free = git_free;

	*out = (git_transport *) t;

#ifdef GIT_WIN32
	ret = WSAStartup(MAKEWORD(2,2), &t->wsd);
	if (ret != 0) {
		git_free(*out);
		giterr_set(GITERR_NET, "Winsock init failed");
		return -1;
	}
#endif

	return 0;

on_error:
	git__free(t);
	return -1;
}
