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
#include "transport.h"
#include "posix.h"
#include "path.h"
#include "buffer.h"
#include "pkt.h"

typedef struct {
	git_transport parent;
	git_repository *repo;
} transport_local;

static int add_ref(transport_local *t, const char *name)
{
	const char peeled[] = "^{}";
	git_remote_head *head;
	git_object *obj = NULL, *target = NULL;
	git_transport *transport = (git_transport *) t;
	git_buf buf = GIT_BUF_INIT;
	git_pkt_ref *pkt;

	head = git__malloc(sizeof(git_remote_head));
	GITERR_CHECK_ALLOC(head);
	pkt = git__malloc(sizeof(git_pkt_ref));
	GITERR_CHECK_ALLOC(pkt);

	head->name = git__strdup(name);
	GITERR_CHECK_ALLOC(head->name);

	if (git_reference_name_to_oid(&head->oid, t->repo, name) < 0) {
		git__free(head);
		git__free(pkt->head.name);
		git__free(pkt);
	}

	pkt->type = GIT_PKT_REF;
	memcpy(&pkt->head, head, sizeof(git_remote_head));
	git__free(head);

	if (git_vector_insert(&transport->refs, pkt) < 0)
	{
		git__free(pkt->head.name);
		git__free(pkt);
		return -1;
	}

	/* If it's not a tag, we don't need to try to peel it */
	if (git__prefixcmp(name, GIT_REFS_TAGS_DIR))
		return 0;

	if (git_object_lookup(&obj, t->repo, &pkt->head.oid, GIT_OBJ_ANY) < 0)
		return -1;

	head = NULL;

	/* If it's not an annotated tag, just get out */
	if (git_object_type(obj) != GIT_OBJ_TAG) {
		git_object_free(obj);
		return 0;
	}

	/* And if it's a tag, peel it, and add it to the list */
	head = git__malloc(sizeof(git_remote_head));
	GITERR_CHECK_ALLOC(head);
	if (git_buf_join(&buf, 0, name, peeled) < 0)
		return -1;

	head->name = git_buf_detach(&buf);

	pkt = git__malloc(sizeof(git_pkt_ref));
	GITERR_CHECK_ALLOC(pkt);
	pkt->type = GIT_PKT_REF;

	if (git_tag_peel(&target, (git_tag *) obj) < 0)
		goto on_error;

	git_oid_cpy(&head->oid, git_object_id(target));
	git_object_free(obj);
	git_object_free(target);
	memcpy(&pkt->head, head, sizeof(git_remote_head));
	git__free(head);

	if (git_vector_insert(&transport->refs, pkt) < 0)
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
	git_transport *transport = (git_transport *) t;

	assert(t);

	if (git_reference_list(&ref_names, t->repo, GIT_REF_LISTALL) < 0 ||
		git_vector_init(&transport->refs, (unsigned int)ref_names.count, NULL) < 0)
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
	git_vector_free(&transport->refs);
	git_strarray_free(&ref_names);
	return -1;
}

/*
 * Try to open the url as a git directory. The direction doesn't
 * matter in this case because we're calulating the heads ourselves.
 */
static int local_connect(git_transport *transport, int direction)
{
	git_repository *repo;
	int error;
	transport_local *t = (transport_local *) transport;
	const char *path;
	git_buf buf = GIT_BUF_INIT;

	GIT_UNUSED(direction);

	/* The repo layer doesn't want the prefix */
	if (!git__prefixcmp(transport->url, "file://")) {
		if (git_path_fromurl(&buf, transport->url) < 0) {
			git_buf_free(&buf);
			return -1;
		}
		path = git_buf_cstr(&buf);

	} else { /* We assume transport->url is already a path */
		path = transport->url;
	}

	error = git_repository_open(&repo, path);

	git_buf_free(&buf);

	if (error < 0)
		return -1;

	t->repo = repo;

	if (store_refs(t) < 0)
		return -1;

	t->parent.connected = 1;

	return 0;
}

static int local_negotiate_fetch(git_transport *transport, git_repository *repo, const git_vector *wants)
{
	GIT_UNUSED(transport);
	GIT_UNUSED(repo);
	GIT_UNUSED(wants);

	giterr_set(GITERR_NET, "Fetch via local transport isn't implemented. Sorry");
	return -1;
}

static int local_close(git_transport *transport)
{
	transport_local *t = (transport_local *)transport;

	t->parent.connected = 0;
	git_repository_free(t->repo);
	t->repo = NULL;

	return 0;
}

static void local_free(git_transport *transport)
{
	unsigned int i;
	transport_local *t = (transport_local *) transport;
	git_vector *vec = &transport->refs;
	git_pkt_ref *pkt;

	assert(transport);

	git_vector_foreach (vec, i, pkt) {
		git__free(pkt->head.name);
		git__free(pkt);
	}
	git_vector_free(vec);

	git__free(t->parent.url);
	git__free(t);
}

/**************
 * Public API *
 **************/

int git_transport_local(git_transport **out)
{
	transport_local *t;

	t = git__malloc(sizeof(transport_local));
	GITERR_CHECK_ALLOC(t);

	memset(t, 0x0, sizeof(transport_local));

	t->parent.own_logic = 1;
	t->parent.connect = local_connect;
	t->parent.negotiate_fetch = local_negotiate_fetch;
	t->parent.close = local_close;
	t->parent.free = local_free;

	*out = (git_transport *) t;

	return 0;
}
