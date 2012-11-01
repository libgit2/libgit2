/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/types.h"
#include "git2/net.h"
#include "git2/repository.h"
#include "git2/object.h"
#include "git2/tag.h"
#include "refs.h"
#include "git2/transport.h"
#include "posix.h"
#include "path.h"
#include "buffer.h"

typedef struct {
	git_transport parent;
	char *url;
	int direction;
	int flags;
	git_atomic cancelled;
	git_repository *repo;
	git_vector refs;
	unsigned connected : 1;
} transport_local;

static int add_ref(transport_local *t, const char *name)
{
	const char peeled[] = "^{}";
	git_remote_head *head;
	git_object *obj = NULL, *target = NULL;
	git_buf buf = GIT_BUF_INIT;

	head = (git_remote_head *)git__malloc(sizeof(git_remote_head));
	GITERR_CHECK_ALLOC(head);

	head->name = git__strdup(name);
	GITERR_CHECK_ALLOC(head->name);

	if (git_reference_name_to_oid(&head->oid, t->repo, name) < 0) {
		git__free(head->name);
		git__free(head);
		return -1;
	}

	if (git_vector_insert(&t->refs, head) < 0)
	{
		git__free(head->name);
		git__free(head);
		return -1;
	}

	/* If it's not a tag, we don't need to try to peel it */
	if (git__prefixcmp(name, GIT_REFS_TAGS_DIR))
		return 0;

	if (git_object_lookup(&obj, t->repo, &head->oid, GIT_OBJ_ANY) < 0)
		return -1;

	head = NULL;

	/* If it's not an annotated tag, just get out */
	if (git_object_type(obj) != GIT_OBJ_TAG) {
		git_object_free(obj);
		return 0;
	}

	/* And if it's a tag, peel it, and add it to the list */
	head = (git_remote_head *)git__malloc(sizeof(git_remote_head));
	GITERR_CHECK_ALLOC(head);
	if (git_buf_join(&buf, 0, name, peeled) < 0)
		return -1;

	head->name = git_buf_detach(&buf);

	if (git_tag_peel(&target, (git_tag *) obj) < 0)
		goto on_error;

	git_oid_cpy(&head->oid, git_object_id(target));
	git_object_free(obj);
	git_object_free(target);

	if (git_vector_insert(&t->refs, head) < 0)
		return -1;

	return 0;

on_error:
	git_object_free(obj);
	git_object_free(target);
	return -1;
}

static int store_refs(transport_local *t)
{
	unsigned int i;
	git_strarray ref_names = {0};

	assert(t);

	if (git_reference_list(&ref_names, t->repo, GIT_REF_LISTALL) < 0 ||
		git_vector_init(&t->refs, (unsigned int)ref_names.count, NULL) < 0)
		goto on_error;

	/* Sort the references first */
	git__tsort((void **)ref_names.strings, ref_names.count, &git__strcmp_cb);

	/* Add HEAD */
	if (add_ref(t, GIT_HEAD_FILE) < 0)
		goto on_error;

	for (i = 0; i < ref_names.count; ++i) {
		if (add_ref(t, ref_names.strings[i]) < 0)
			goto on_error;
	}

	git_strarray_free(&ref_names);
	return 0;

on_error:
	git_vector_free(&t->refs);
	git_strarray_free(&ref_names);
	return -1;
}

/*
 * Try to open the url as a git directory. The direction doesn't
 * matter in this case because we're calulating the heads ourselves.
 */
static int local_connect(git_transport *transport, const char *url, int direction, int flags)
{
	git_repository *repo;
	int error;
	transport_local *t = (transport_local *) transport;
	const char *path;
	git_buf buf = GIT_BUF_INIT;

	t->url = git__strdup(url);
	GITERR_CHECK_ALLOC(t->url);
	t->direction = direction;
	t->flags = flags;

	/* The repo layer doesn't want the prefix */
	if (!git__prefixcmp(t->url, "file://")) {
		if (git_path_fromurl(&buf, t->url) < 0) {
			git_buf_free(&buf);
			return -1;
		}
		path = git_buf_cstr(&buf);

	} else { /* We assume transport->url is already a path */
		path = t->url;
	}

	error = git_repository_open(&repo, path);

	git_buf_free(&buf);

	if (error < 0)
		return -1;

	t->repo = repo;

	if (store_refs(t) < 0)
		return -1;

	t->connected = 1;

	return 0;
}

static int local_ls(git_transport *transport, git_headlist_cb list_cb, void *payload)
{
	transport_local *t = (transport_local *)transport;
	unsigned int i;
	git_remote_head *head = NULL;

	if (!t->connected) {
		giterr_set(GITERR_NET, "The transport is not connected");
		return -1;
	}

	git_vector_foreach(&t->refs, i, head) {
		if (list_cb(head, payload))
			return GIT_EUSER;
	}

	return 0;
}

static int local_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_remote_head * const *refs, size_t count)
{
	GIT_UNUSED(transport);
	GIT_UNUSED(repo);
	GIT_UNUSED(refs);
	GIT_UNUSED(count);

	giterr_set(GITERR_NET, "Fetch via local transport isn't implemented. Sorry");
	return -1;
}

static int local_is_connected(git_transport *transport, int *connected)
{
	transport_local *t = (transport_local *)transport;

	*connected = t->connected;

	return 0;
}

static int local_read_flags(git_transport *transport, int *flags)
{
	transport_local *t = (transport_local *)transport;

	*flags = t->flags;

	return 0;
}

static void local_cancel(git_transport *transport)
{
	transport_local *t = (transport_local *)transport;

	git_atomic_set(&t->cancelled, 1);
}

static int local_close(git_transport *transport)
{
	transport_local *t = (transport_local *)transport;

	t->connected = 0;
	git_repository_free(t->repo);
	t->repo = NULL;

	return 0;
}

static void local_free(git_transport *transport)
{
	unsigned int i;
	transport_local *t = (transport_local *) transport;
	git_vector *vec = &t->refs;
	git_remote_head *head;

	assert(transport);

	git_vector_foreach (vec, i, head) {
		git__free(head->name);
		git__free(head);
	}
	git_vector_free(vec);

	git__free(t->url);
	git__free(t);
}

/**************
 * Public API *
 **************/

int git_transport_local(git_transport **out, void *param)
{
	transport_local *t;

	GIT_UNUSED(param);

	t = git__malloc(sizeof(transport_local));
	GITERR_CHECK_ALLOC(t);

	memset(t, 0x0, sizeof(transport_local));
		
	t->parent.connect = local_connect;
	t->parent.negotiate_fetch = local_negotiate_fetch;
	t->parent.close = local_close;
	t->parent.free = local_free;
	t->parent.ls = local_ls;
	t->parent.is_connected = local_is_connected;
	t->parent.read_flags = local_read_flags;
	t->parent.cancel = local_cancel;

	*out = (git_transport *) t;

	return 0;
}
