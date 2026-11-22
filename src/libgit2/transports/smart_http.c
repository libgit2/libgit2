/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "http.h"

#ifdef GIT_HTTP

#include "smart.h"

/*
 * The smart HTTP transport.  Each step of the protocol is its own
 * request, so every request gets a new stream from the HTTP backend.
 */

typedef struct {
	git_transport_smart_base parent;
	git_http_backend *backend;
	git_transport_smart_stream *stream;
} smart_http_transport;

static void free_stream(smart_http_transport *t)
{
	if (t->stream) {
		t->stream->free(t->stream);
		t->stream = NULL;
	}
}

static int open_stream(
	smart_http_transport *t,
	git_smart_service_t service)
{
	free_stream(t);

	return git_http_backend__open(&t->stream, t->backend, t->parent.session->url, service);
}

static int smart_http_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	smart_http_transport *t = GIT_CONTAINER_OF(transport, smart_http_transport, parent.parent);
	git_smart_service_t service;
	int error;

	free_stream(t);

	if ((error = git_http_backend__close(t->backend)) < 0)
		return error;

	git_transport_smart_session_reset(t->parent.session);

	if ((error = git_transport_smart_base_prepare_connect(&t->parent, url, direction, connect_opts)) < 0)
		return error;

	if (direction == GIT_DIRECTION_FETCH) {
		service = GIT_SERVICE_UPLOADPACK_LS;
	} else if (direction == GIT_DIRECTION_PUSH) {
		service = GIT_SERVICE_RECEIVEPACK_LS;
	} else {
		git_error_set(GIT_ERROR_NET, "invalid direction");
		return -1;
	}

	if ((error = open_stream(t, service)) < 0 ||
	    (error = git_transport_smart_read_advertisement(t->parent.session, t->stream)) < 0)
		return error;

	free_stream(t);

	t->parent.session->connected = 1;

	return 0;
}

static int smart_http_round(
	git_transport_smart_stream **out,
	const char *data,
	size_t len,
	void *payload)
{
	smart_http_transport *t = payload;
	int error;

	if ((error = open_stream(t, GIT_SERVICE_UPLOADPACK)) < 0 ||
	    (error = t->stream->write(t->stream, data, len)) < 0)
		return error;

	*out = t->stream;
	return 0;
}

static int smart_http_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_fetch_negotiation *wants)
{
	smart_http_transport *t = GIT_CONTAINER_OF(transport, smart_http_transport, parent.parent);
	int error;

	if ((error = git_transport_smart_base_ensure_direction(&t->parent, GIT_DIRECTION_FETCH)) < 0)
		return error;

	return git_transport_smart_negotiate(t->parent.session, repo, wants, smart_http_round, t);
}

static int smart_http_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	smart_http_transport *t = GIT_CONTAINER_OF(transport, smart_http_transport, parent.parent);

	return git_transport_smart_download_pack(t->parent.session, t->stream, repo, stats);
}

static int smart_http_push(git_transport *transport, git_push *push)
{
	smart_http_transport *t = GIT_CONTAINER_OF(transport, smart_http_transport, parent.parent);
	int error;

	if ((error = git_transport_smart_push_prepare(t->parent.session, push)) < 0 ||
	    (error = git_transport_smart_base_ensure_direction(&t->parent, GIT_DIRECTION_PUSH)) < 0 ||
	    (error = open_stream(t, GIT_SERVICE_RECEIVEPACK)) < 0)
		return error;

	return git_transport_smart_push_send(t->parent.session, t->stream, push);
}

static int smart_http_close(git_transport *transport)
{
	smart_http_transport *t = GIT_CONTAINER_OF(transport, smart_http_transport, parent.parent);
	int error;

	free_stream(t);

	error = git_http_backend__close(t->backend);

	git_transport_smart_session_close(t->parent.session);

	t->parent.session->connected = 0;

	return error;
}

static void smart_http_free(git_transport *transport)
{
	smart_http_transport *t = GIT_CONTAINER_OF(transport, smart_http_transport, parent.parent);

	smart_http_close(transport);

	git_http_backend__free(t->backend);
	git_transport_smart_base_dispose(&t->parent);
	git__free(t);
}

int git_transport_http(git_transport **out, git_remote *owner, void *param)
{
	smart_http_transport *t;

	GIT_UNUSED(param);

	GIT_ASSERT_ARG(out);

	t = git__calloc(1, sizeof(smart_http_transport));
	GIT_ERROR_CHECK_ALLOC(t);

	if (git_transport_smart_base_init(&t->parent, owner, true) < 0 ||
	    git_http_backend__new(&t->backend, &t->parent) < 0) {
		git_transport_smart_base_dispose(&t->parent);
		git__free(t);
		return -1;
	}

	t->parent.parent.connect = smart_http_connect;
	t->parent.parent.negotiate_fetch = smart_http_negotiate_fetch;
	t->parent.parent.download_pack = smart_http_download_pack;
	t->parent.parent.push = smart_http_push;
	t->parent.parent.close = smart_http_close;
	t->parent.parent.free = smart_http_free;

	*out = &t->parent.parent;
	return 0;
}

#endif /* GIT_HTTP */
