/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "smart.h"

/*
 * Methods shared by transports that keep one stream open for the whole
 * conversation.  Those transports implement `connect` by calling
 * `begin_connect`, opening their stream and calling `finish_connect`.
 */

static void free_stream(git_transport_smart_stateful *t)
{
	if (t->stream) {
		t->stream->free(t->stream);
		t->stream = NULL;
	}
}

int git_transport_smart_stateful__begin_connect(
	git_transport_smart_stateful *t,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	int error;

	free_stream(t);
	git_transport_smart_session_reset(t->parent.session);

	if ((error = git_transport_smart_base_prepare_connect(&t->parent, url, direction, connect_opts)) < 0)
		return error;

	if (direction != GIT_DIRECTION_FETCH && direction != GIT_DIRECTION_PUSH) {
		git_error_set(GIT_ERROR_NET, "invalid direction");
		return -1;
	}

	return 0;
}

int git_transport_smart_stateful__finish_connect(git_transport_smart_stateful *t)
{
	int error;

	if ((error = git_transport_smart_read_advertisement(t->parent.session, t->stream)) < 0)
		return error;

	t->parent.session->connected = 1;

	return 0;
}

/* Find the stream that was opened by connecting in the given direction. */
static int stream_for(
	git_transport_smart_stream **out,
	git_transport_smart_stateful *t,
	int direction)
{
	int error;

	if ((error = git_transport_smart_base_ensure_direction(&t->parent, direction)) < 0)
		return error;

	if (!t->stream) {
		git_error_set(GIT_ERROR_NET, "the transport is not connected");
		return -1;
	}

	*out = t->stream;
	return 0;
}

static int stateful_round(
	git_transport_smart_stream **out,
	const char *data,
	size_t len,
	void *payload)
{
	git_transport_smart_stream *stream = payload;

	*out = stream;

	return stream->write(stream, data, len);
}

static int stateful_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_fetch_negotiation *wants)
{
	git_transport_smart_stateful *t = GIT_CONTAINER_OF(transport, git_transport_smart_stateful, parent.parent);
	git_transport_smart_stream *stream;
	int error;

	if ((error = stream_for(&stream, t, GIT_DIRECTION_FETCH)) < 0)
		return error;

	return git_transport_smart_negotiate(t->parent.session, repo, wants, stateful_round, stream);
}

static int stateful_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	git_transport_smart_stateful *t = GIT_CONTAINER_OF(transport, git_transport_smart_stateful, parent.parent);
	git_transport_smart_stream *stream;
	int error;

	if ((error = stream_for(&stream, t, GIT_DIRECTION_FETCH)) < 0)
		return error;

	return git_transport_smart_download_pack(t->parent.session, stream, repo, stats);
}

static int stateful_push(git_transport *transport, git_push *push)
{
	git_transport_smart_stateful *t = GIT_CONTAINER_OF(transport, git_transport_smart_stateful, parent.parent);
	git_transport_smart_stream *stream;
	int error;

	if ((error = git_transport_smart_push_prepare(t->parent.session, push)) < 0 ||
	    (error = stream_for(&stream, t, GIT_DIRECTION_PUSH)) < 0)
		return error;

	return git_transport_smart_push_send(t->parent.session, stream, push);
}

static int stateful_close(git_transport *transport)
{
	git_transport_smart_stateful *t = GIT_CONTAINER_OF(transport, git_transport_smart_stateful, parent.parent);

	/*
	 * If we're still connected, say goodbye by sending a flush, or
	 * git-daemon will complain that we disconnected unexpectedly.
	 */
	if (t->parent.session->connected && t->stream)
		git_transport_smart_goodbye(t->stream);

	free_stream(t);
	git_transport_smart_session_close(t->parent.session);

	t->parent.session->connected = 0;

	return 0;
}

void git_transport_smart_stateful__free(git_transport *transport)
{
	git_transport_smart_stateful *t = GIT_CONTAINER_OF(transport, git_transport_smart_stateful, parent.parent);

	stateful_close(transport);

	git_transport_smart_base_dispose(&t->parent);
	git__free(t);
}

int git_transport_smart_stateful__init(
	git_transport_smart_stateful *t,
	git_remote *owner)
{
	t->parent.parent.negotiate_fetch = stateful_negotiate_fetch;
	t->parent.parent.download_pack = stateful_download_pack;
	t->parent.parent.push = stateful_push;
	t->parent.parent.close = stateful_close;
	t->parent.parent.free = git_transport_smart_stateful__free;

	return git_transport_smart_base_init(&t->parent, owner, false);
}
