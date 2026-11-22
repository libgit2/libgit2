/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "smart_api.h"

/*
 * Adapts a caller-provided `git_smart_subtransport` to a transport.  This
 * uses only the interface in `smart_api.h`, as any other transport would.
 */

typedef struct {
	git_transport_smart_base parent;
	git_smart_subtransport *wrapped;
	git_transport_smart_stream *current_stream;
	int direction;
	unsigned rpc : 1;
} smart_subtransport_adapter;

GIT_INLINE(void) free_stream(smart_subtransport_adapter *t)
{
	if (t->current_stream) {
		t->current_stream->free(t->current_stream);
		t->current_stream = NULL;
	}
}

static int adapter_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	smart_subtransport_adapter *t = GIT_CONTAINER_OF(transport, smart_subtransport_adapter, parent.parent);
	git_transport_smart_stream *stream;
	git_smart_service_t service;
	int error;

	free_stream(t);

	if (t->wrapped->close(t->wrapped) < 0)
		return -1;

	git_transport_smart_session_reset(t->parent.session);

	if (git_transport_smart_base_prepare_connect(&t->parent, url, direction, connect_opts) < 0)
		return -1;

	t->direction = direction;

	if (GIT_DIRECTION_FETCH == direction) {
		service = GIT_SERVICE_UPLOADPACK_LS;
	} else if (GIT_DIRECTION_PUSH == direction) {
		service = GIT_SERVICE_RECEIVEPACK_LS;
	} else {
		git_error_set(GIT_ERROR_NET, "invalid direction");
		return -1;
	}

	if ((error = t->wrapped->action(&stream, t->wrapped, url, service)) < 0)
		return error;

	/* Save off the current stream (i.e. socket) that we are working with */
	t->current_stream = stream;

	if ((error = git_transport_smart_read_advertisement(t->parent.session, stream)) < 0)
		return error;

	if (t->rpc)
		free_stream(t);

	/* We're now logically connected. */
	git_transport_smart_base_set_connected(&t->parent, 1);

	return 0;
}

/* Ask the subtransport for the stream to use for the given service. */
static int adapter_action(
	git_transport_smart_stream **out,
	smart_subtransport_adapter *t,
	int direction,
	git_smart_service_t service)
{
	int error;

	if (t->rpc)
		free_stream(t);

	if ((error = git_transport_smart_base_ensure_direction(&t->parent, direction)) < 0)
		return error;

	if ((error = t->wrapped->action(out, t->wrapped,
			git_transport_smart_base_url(&t->parent), service)) < 0)
		return error;

	/* If this is a stateful implementation, the stream we get back should be the same */
	GIT_ASSERT(t->rpc || t->current_stream == *out);

	/* Save off the current stream (i.e. socket) that we are working with */
	t->current_stream = *out;

	return 0;
}

static int adapter_round(
	git_transport_smart_stream **out,
	const char *data,
	size_t len,
	void *payload)
{
	smart_subtransport_adapter *t = payload;
	git_transport_smart_stream *stream;
	int error;

	if ((error = adapter_action(&stream, t, GIT_DIRECTION_FETCH, GIT_SERVICE_UPLOADPACK)) < 0 ||
	    (error = stream->write(stream, data, len)) < 0)
		return error;

	*out = stream;
	return 0;
}

static int adapter_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_fetch_negotiation *wants)
{
	smart_subtransport_adapter *t = GIT_CONTAINER_OF(transport, smart_subtransport_adapter, parent.parent);

	return git_transport_smart_negotiate(t->parent.session, repo, wants, adapter_round, t);
}

static int adapter_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	smart_subtransport_adapter *t = GIT_CONTAINER_OF(transport, smart_subtransport_adapter, parent.parent);

	return git_transport_smart_download_pack(t->parent.session, t->current_stream, repo, stats);
}

static int adapter_push(git_transport *transport, git_push *push)
{
	smart_subtransport_adapter *t = GIT_CONTAINER_OF(transport, smart_subtransport_adapter, parent.parent);
	git_transport_smart_stream *stream;
	int error;

	if ((error = git_transport_smart_push_prepare(t->parent.session, push)) < 0 ||
	    (error = adapter_action(&stream, t, GIT_DIRECTION_PUSH, GIT_SERVICE_RECEIVEPACK)) < 0)
		return error;

	return git_transport_smart_push_send(t->parent.session, stream, push);
}

static int adapter_close(git_transport *transport)
{
	smart_subtransport_adapter *t = GIT_CONTAINER_OF(transport, smart_subtransport_adapter, parent.parent);
	git_transport_smart_stream *stream;
	git_smart_service_t service;
	int ret;

	if (t->direction == GIT_DIRECTION_FETCH) {
		service = GIT_SERVICE_UPLOADPACK;
	} else if (t->direction == GIT_DIRECTION_PUSH) {
		service = GIT_SERVICE_RECEIVEPACK;
	} else {
		git_error_set(GIT_ERROR_NET, "invalid direction");
		return -1;
	}

	/*
	 * If we're still connected at this point and not using RPC,
	 * we should say goodbye by sending a flush, or git-daemon
	 * will complain that we disconnected unexpectedly.
	 */
	if (transport->is_connected(transport) && !t->rpc &&
	    !t->wrapped->action(&stream, t->wrapped,
			git_transport_smart_base_url(&t->parent), service)) {
		git_transport_smart_goodbye(t->current_stream);
	}

	free_stream(t);

	ret = t->wrapped->close(t->wrapped);

	git_transport_smart_session_close(t->parent.session);
	git_transport_smart_base_set_connected(&t->parent, 0);

	return ret < 0 ? -1 : 0;
}

static void adapter_free(git_transport *transport)
{
	smart_subtransport_adapter *t = GIT_CONTAINER_OF(transport, smart_subtransport_adapter, parent.parent);

	/* Make sure that the current stream is closed, if we have one. */
	adapter_close(transport);

	/* Free the subtransport */
	t->wrapped->free(t->wrapped);

	git_transport_smart_base_dispose(&t->parent);
	git__free(t);
}

int git_transport_smart(git_transport **out, git_remote *owner, void *param)
{
	smart_subtransport_adapter *t;
	git_smart_subtransport_definition *definition = (git_smart_subtransport_definition *)param;

	if (!param)
		return -1;

	t = git__calloc(1, sizeof(smart_subtransport_adapter));
	GIT_ERROR_CHECK_ALLOC(t);

	t->rpc = definition->rpc;

	if (git_transport_smart_base_init(&t->parent, owner, definition->rpc) < 0)
		goto on_error;

	t->parent.parent.connect = adapter_connect;
	t->parent.parent.negotiate_fetch = adapter_negotiate_fetch;
	t->parent.parent.download_pack = adapter_download_pack;
	t->parent.parent.push = adapter_push;
	t->parent.parent.close = adapter_close;
	t->parent.parent.free = adapter_free;

	if (definition->callback(&t->wrapped, &t->parent.parent, definition->param) < 0)
		goto on_error;

	*out = &t->parent.parent;
	return 0;

on_error:
	git_transport_smart_base_dispose(&t->parent);
	git__free(t);
	return -1;
}
