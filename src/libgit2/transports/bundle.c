/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "bundle.h"
#include "remote.h"

#include "git2/types.h"
#include "git2/transport.h"

typedef struct {
	git_transport parent;
	git_remote *owner;
	char *url;
	int direction;
	git_atomic32 cancelled;
	git_bundle_header *header;
	git_remote_connect_options connect_opts;
	unsigned connected : 1;
} transport_bundle;

/*
 * HEAD needs to be a the front of the ls_remote call. At least clone
 * assumes it its.
 */
static int sort_head_to_front(git_vector *refs)
{
	size_t idx = 0;
	git_remote_head *ref;
	git_vector sortedRefs = GIT_VECTOR_INIT;

	git_vector_foreach (refs, idx, ref) {
		if (git__strcmp(ref->name, "HEAD") == 0) {
			if (git_vector_insert(&sortedRefs, ref) < 0) {
				return -1;
			}
			break;
		}
	}

	idx = 0;
	git_vector_foreach (refs, idx, ref) {
		if (git__strcmp(ref->name, "HEAD") != 0) {
			if (git_vector_insert(&sortedRefs, ref) < 0) {
				return -1;
			}
		}
	}

	git_vector_swap(&sortedRefs, refs);
	git_vector_dispose(&sortedRefs);

	return 0;
}

static int bundle_set_connect_opts(
        git_transport *transport,
        const git_remote_connect_options *connect_opts)
{
	transport_bundle *t = (transport_bundle *)transport;

	if (!t->connected) {
		git_error_set(
		        GIT_ERROR_NET,
		        "cannot reconfigure a transport that is not connected");
		return -1;
	}

	return git_remote_connect_options_normalize(
	        &t->connect_opts, t->owner->repo, connect_opts);
}

static int bundle_connect(
        git_transport *transport,
        const char *url,
        int direction,
        const git_remote_connect_options *connect_opts)
{
	int error;
	transport_bundle *t = (transport_bundle *)transport;

	if (t->connected)
		return 0;

	if (direction == GIT_DIRECTION_PUSH) {
		git_error_set(
		        GIT_ERROR_NET,
		        "push is not supported by bundle transport");
		return GIT_ENOTSUPPORTED;
	}

	if (git_remote_connect_options_normalize(
	            &t->connect_opts, t->owner->repo, connect_opts) < 0)
		return -1;

	/*
	 * Need to free here. We cannot free in "bundle_close" because
	 * "bundle_ls" gets called after disconnection.
	 */
	git_bundle_header_free(t->header);

	t->url = git__strdup(url);
	GIT_ERROR_CHECK_ALLOC(t->url);
	t->direction = direction;

	if ((error = git_bundle_header_open(&t->header, url)) < 0 ||
	    (error = sort_head_to_front(&t->header->refs)) < 0) {
		return error;
	}

	t->connected = 1;

	return 0;
}

static int bundle_negotiate_fetch(
        git_transport *transport,
        git_repository *repo,
        const git_fetch_negotiation *wants)
{
	transport_bundle *t = (transport_bundle *)transport;
	git_remote_head *rhead;
	unsigned int i;

	if (wants->depth) {
		git_error_set(
		        GIT_ERROR_NET,
		        "shallow fetch is not supported by bundle transport");
		return GIT_ENOTSUPPORTED;
	}

	/* Fill in the loids */
	git_vector_foreach (&t->header->refs, i, rhead) {
		git_object *obj;

		int error = git_revparse_single(&obj, repo, rhead->name);
		if (!error)
			git_oid_cpy(&rhead->loid, git_object_id(obj));
		else if (error != GIT_ENOTFOUND)
			return error;
		else
			git_error_clear();
		git_object_free(obj);
	}

	return 0;
}

static int
bundle_capabilities(unsigned int *capabilities, git_transport *transport)
{
	GIT_UNUSED(transport);

	*capabilities = GIT_REMOTE_CAPABILITY_TIP_OID |
	                GIT_REMOTE_CAPABILITY_REACHABLE_OID;
	return 0;
}

static int bundle_download_pack(
        git_transport *transport,
        git_repository *repo,
        git_indexer_progress *stats)
{
	transport_bundle *t = (transport_bundle *)transport;

	return git_bundle__read_pack(repo, t->url, stats);
}

static int bundle_shallow_roots(git_oidarray *out, git_transport *transport)
{
	GIT_UNUSED(out);
	GIT_UNUSED(transport);

	return 0;
}

static int bundle_push(git_transport *transport, git_push *push)
{
	GIT_UNUSED(transport);
	GIT_UNUSED(push);

	git_error_set(
	        GIT_ERROR_NET, "push is not supported by bundle transport");
	return GIT_ENOTSUPPORTED;
}

static int
bundle_ls(const git_remote_head ***out, size_t *size, git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	*out = (const git_remote_head **)t->header->refs.contents;
	*size = t->header->refs.length;
	return 0;
}

static int bundle_is_connected(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	return t->connected;
}

static void bundle_cancel(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	git_atomic32_set(&t->cancelled, 1);
}

static int bundle_close(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	t->connected = 0;

	if (t->url) {
		git__free(t->url);
		t->url = NULL;
	}

	return 0;
}

static void bundle_free(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	/* Close the transport, if it's still open. */
	bundle_close(transport);

	git_bundle_header_free(t->header);

	/* Free the transport */
	git__free(t);
}

#ifdef GIT_EXPERIMENTAL_SHA256
static int bundle_oid_type(git_oid_t *out, git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	*out = t->header->oid_type;

	return 0;
}
#endif

int git_transport_bundle(git_transport **out, git_remote *owner, void *param)
{
	int error = 0;
	transport_bundle *t;

	GIT_UNUSED(param);

	t = git__calloc(1, sizeof(transport_bundle));
	GIT_ERROR_CHECK_ALLOC(t);

	t->parent.version = GIT_TRANSPORT_VERSION;
	t->parent.connect = bundle_connect;
	t->parent.set_connect_opts = bundle_set_connect_opts;
	t->parent.capabilities = bundle_capabilities;
#ifdef GIT_EXPERIMENTAL_SHA256
	t->parent.oid_type = bundle_oid_type;
#endif
	t->parent.negotiate_fetch = bundle_negotiate_fetch;
	t->parent.shallow_roots = bundle_shallow_roots;
	t->parent.download_pack = bundle_download_pack;
	t->parent.push = bundle_push;
	t->parent.close = bundle_close;
	t->parent.free = bundle_free;
	t->parent.ls = bundle_ls;
	t->parent.is_connected = bundle_is_connected;
	t->parent.cancel = bundle_cancel;

	t->owner = owner;

	*out = (git_transport *)t;

	return error;
}
