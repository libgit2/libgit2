/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "smartnew.h"
#include "netops.h"
#include "stream.h"
#include "streams/socket.h"
#include "git2/sys/transport.h"

typedef struct {
	git_transport parent;
	git_smart smart;

	git_remote *owner;
} transport_git;

static int transport_git_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	transport_git *t = (transport_git *)transport;
	git_net_url urldata = GIT_NET_URL_INIT;
	int error;

	if ((error = git_remote_connect_options_normalize(&t->connect_opts,
		t->owner->repo, connect_opts)) < 0 ||
	    (error = git_net_url_parse(&urldata, url) < 0) ||
	    (error = git_socket_stream_new(&t->stream, host, port)) < 0 ||
	    (error = git_stream_connect(t->stream)) < 0)
		goto done;

done:
	git_net_url_dispose(&urldata);
	return error;
}

//
static int git_smart__connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	transport_smart *t = GIT_CONTAINER_OF(transport, transport_smart, parent);
	git_smart_subtransport_stream *stream;
	int error;
	git_pkt *pkt;
	git_pkt_ref *first;
	git_vector symrefs;
	git_smart_service_t service;

	if (git_smart__reset_stream(t, true) < 0)
		return -1;

	t->url = git__strdup(url);
	GIT_ERROR_CHECK_ALLOC(t->url);

	t->direction = direction;

	if (GIT_DIRECTION_FETCH == t->direction) {
		service = GIT_SERVICE_UPLOADPACK_LS;
	} else if (GIT_DIRECTION_PUSH == t->direction) {
		service = GIT_SERVICE_RECEIVEPACK_LS;
	} else {
		git_error_set(GIT_ERROR_NET, "invalid direction");
		return -1;
	}

	if ((error = t->wrapped->action(&stream, t->wrapped, t->url, service)) < 0)
		return error;

	/* Save off the current stream (i.e. socket) that we are working with */
	t->current_stream = stream;

	gitno_buffer_setup_callback(&t->buffer, t->buffer_data, sizeof(t->buffer_data), git_smart__recv_cb, t);

	/* 2 flushes for RPC; 1 for stateful */
	if ((error = git_smart__store_refs(t, t->rpc ? 2 : 1)) < 0)
		return error;

	/* Strip the comment packet for RPC */
	if (t->rpc) {
		pkt = (git_pkt *)git_vector_get(&t->refs, 0);

		if (!pkt || GIT_PKT_COMMENT != pkt->type) {
			git_error_set(GIT_ERROR_NET, "invalid response");
			return -1;
		} else {
			/* Remove the comment pkt from the list */
			git_vector_remove(&t->refs, 0);
			git__free(pkt);
		}
	}

	/* We now have loaded the refs. */
	t->have_refs = 1;

	pkt = (git_pkt *)git_vector_get(&t->refs, 0);
	if (pkt && GIT_PKT_REF != pkt->type) {
		git_error_set(GIT_ERROR_NET, "invalid response");
		return -1;
	}
	first = (git_pkt_ref *)pkt;

	if ((error = git_vector_init(&symrefs, 1, NULL)) < 0)
		return error;

	/* Detect capabilities */
	if ((error = git_smart__detect_caps(first, &t->caps, &symrefs)) == 0) {
		/* If the only ref in the list is capabilities^{} with OID_ZERO, remove it */
		if (1 == t->refs.length && !strcmp(first->head.name, "capabilities^{}") &&
			git_oid_is_zero(&first->head.oid)) {
			git_vector_clear(&t->refs);
			git_pkt_free((git_pkt *)first);
		}

		/* Keep a list of heads for _ls */
		git_smart__update_heads(t, &symrefs);
	} else if (error == GIT_ENOTFOUND) {
		/* There was no ref packet received, or the cap list was empty */
		error = 0;
	} else {
		git_error_set(GIT_ERROR_NET, "invalid response");
		goto cleanup;
	}

	if (t->rpc && (error = git_smart__reset_stream(t, false)) < 0)
		goto cleanup;

	/* We're now logically connected. */
	t->connected = 1;

cleanup:
	free_symrefs(&symrefs);

	return error;
}

static int transport_git_set_connect_opts(
	git_transport *transport,
	const git_remote_connect_options *connect_opts)
{
	transport_git *t = (transport_git *)transport;
	return -1;
}

static int transport_git_capabilities(
	unsigned int *capabilities,
	git_transport *transport)
{
	transport_git *t = (transport_git *)transport;
	return -1;
}

#ifdef GIT_EXPERIMENTAL_SHA256

static int transport_git_oid_type(
	git_oid_t *object_type,
	git_transport *transport)
{
	transport_git *t = (transport_git *)transport;
	return -1;
}

#endif

static int transport_git_ls(
	const git_remote_head ***out,
	size_t *size,
	git_transport *transport)
{
	transport_git *t = (transport_git *)transport;
	return -1;
}

static int transport_git_push(
	git_transport *transport,
	git_push *push)
{
	transport_git *t = (transport_git *)transport;
	return -1;
}

static int transport_git_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_remote_head * const *refs,
	size_t count)
{
	transport_git *t = (transport_git *)transport;
	return -1;
}

static int transport_git_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	transport_git *t = (transport_git *)transport;
	return -1;
}

static void transport_git_cancel(git_transport *transport)
{
	transport_git *t = (transport_git *)transport;

	return;
}

static int transport_git_is_connected(git_transport *transport)
{
	transport_git *t = (transport_git *)transport;

	return 0;
}

static int transport_git_close(git_transport *transport)
{
	transport_git *t = (transport_git *)transport;

	return git_smart_close(&t->smart);
}

static void transport_git_free(git_transport *transport)
{
	transport_git *t = (transport_git *)transport;

	transport_git_close(transport);

	git_smart_dispose(&t->smart);
	git__free(t);
}

int git_transport_git(git_transport **out, git_remote *owner, void *param)
{
	transport_git *t;

	t = git__calloc(1, sizeof(transport_git));
	GIT_ERROR_CHECK_ALLOC(t);

	if (git_smart_init(&t->smart) < 0) {
		git__free(t);
		return -1;
	}

	t->parent.version = GIT_TRANSPORT_VERSION;
	t->parent.connect = transport_git_connect;
	t->parent.set_connect_opts = transport_git_set_connect_opts;
	t->parent.capabilities = transport_git_capabilities;
#ifdef GIT_EXPERIMENTAL_SHA256
	t->parent.oid_type = transport_git_oid_type;
#endif
	t->parent.negotiate_fetch = transport_git_negotiate_fetch;
	t->parent.download_pack = transport_git_download_pack;
	t->parent.push = transport_git_push;
	t->parent.ls = transport_git_ls;
	t->parent.is_connected = transport_git_is_connected;
	t->parent.cancel = transport_git_cancel;
	t->parent.close = transport_git_close;
	t->parent.free = transport_git_free;

	t->owner = owner;

	*out = (git_transport *)t;
	return 0;
}
