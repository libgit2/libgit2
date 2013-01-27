/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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
#include "git2/transport.h"
#include "git2/revwalk.h"
#include "git2/odb_backend.h"
#include "git2/pack.h"
#include "git2/commit.h"
#include "git2/revparse.h"
#include "pack-objects.h"
#include "refs.h"
#include "posix.h"
#include "path.h"
#include "buffer.h"
#include "repository.h"
#include "odb.h"

typedef struct {
	git_transport parent;
	git_remote *owner;
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
	int error;

	head = git__calloc(1, sizeof(git_remote_head));
	GITERR_CHECK_ALLOC(head);

	head->name = git__strdup(name);
	GITERR_CHECK_ALLOC(head->name);

	error = git_reference_name_to_id(&head->oid, t->repo, name);
	if (error < 0) {
		git__free(head->name);
		git__free(head);
		if (!strcmp(name, GIT_HEAD_FILE) && error == GIT_ENOTFOUND) {
			/* This is actually okay.  Empty repos often have a HEAD that points to
			 * a nonexistent "refs/heads/master". */
			giterr_clear();
			return 0;
		}
		return error;
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
	head = git__calloc(1, sizeof(git_remote_head));
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
static int local_connect(
	git_transport *transport,
	const char *url,
	git_cred_acquire_cb cred_acquire_cb,
	void *cred_acquire_payload,
	int direction, int flags)
{
	git_repository *repo;
	int error;
	transport_local *t = (transport_local *) transport;
	const char *path;
	git_buf buf = GIT_BUF_INIT;

	GIT_UNUSED(cred_acquire_cb);
	GIT_UNUSED(cred_acquire_payload);

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
	const git_remote_head * const *refs,
	size_t count)
{
	transport_local *t = (transport_local*)transport;
	git_remote_head *rhead;
	unsigned int i;

	GIT_UNUSED(refs);
	GIT_UNUSED(count);

	/* Fill in the loids */
	git_vector_foreach(&t->refs, i, rhead) {
		git_object *obj;

		int error = git_revparse_single(&obj, repo, rhead->name);
		if (!error)
			git_oid_cpy(&rhead->loid, git_object_id(obj));
		else if (error != GIT_ENOTFOUND)
			return error;
		git_object_free(obj);
		giterr_clear();
	}

	return 0;
}

typedef struct foreach_data {
	git_transfer_progress *stats;
	git_transfer_progress_callback progress_cb;
	void *progress_payload;
	git_odb_writepack *writepack;
} foreach_data;

static int foreach_cb(void *buf, size_t len, void *payload)
{
	foreach_data *data = (foreach_data*)payload;

	data->stats->received_bytes += len;
	return data->writepack->add(data->writepack, buf, len, data->stats);
}

static int local_download_pack(
		git_transport *transport,
		git_repository *repo,
		git_transfer_progress *stats,
		git_transfer_progress_callback progress_cb,
		void *progress_payload)
{
	transport_local *t = (transport_local*)transport;
	git_revwalk *walk = NULL;
	git_remote_head *rhead;
	unsigned int i;
	int error = -1;
	git_oid oid;
	git_packbuilder *pack = NULL;
	git_odb_writepack *writepack = NULL;
	git_odb *odb = NULL;

	if ((error = git_revwalk_new(&walk, t->repo)) < 0)
		goto cleanup;
	git_revwalk_sorting(walk, GIT_SORT_TIME);

	if ((error = git_packbuilder_new(&pack, t->repo)) < 0)
		goto cleanup;

	stats->total_objects = 0;
	stats->indexed_objects = 0;
	stats->received_objects = 0;
	stats->received_bytes = 0;

	git_vector_foreach(&t->refs, i, rhead) {
		git_object *obj;
		if ((error = git_object_lookup(&obj, t->repo, &rhead->oid, GIT_OBJ_ANY)) < 0)
			goto cleanup;

		if (git_object_type(obj) == GIT_OBJ_COMMIT) {
			/* Revwalker includes only wanted commits */
			error = git_revwalk_push(walk, &rhead->oid);
			if (!git_oid_iszero(&rhead->loid))
				error = git_revwalk_hide(walk, &rhead->loid);
		} else {
			/* Tag or some other wanted object. Add it on its own */
			error = git_packbuilder_insert(pack, &rhead->oid, rhead->name);
		}
      git_object_free(obj);
	}

	/* Walk the objects, building a packfile */
	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0)
		goto cleanup;

	while ((error = git_revwalk_next(&oid, walk)) == 0) {
		git_commit *commit;

		/* Skip commits we already have */
		if (git_odb_exists(odb, &oid)) continue;

		if (!git_object_lookup((git_object**)&commit, t->repo, &oid, GIT_OBJ_COMMIT)) {
			const git_oid *tree_oid = git_commit_tree_id(commit);

			/* Add the commit and its tree */
			if ((error = git_packbuilder_insert(pack, &oid, NULL)) < 0 ||
				 (error = git_packbuilder_insert_tree(pack, tree_oid)) < 0) {
				git_commit_free(commit);
				goto cleanup;
			}

			git_commit_free(commit);
		}
	}

	if ((error = git_odb_write_pack(&writepack, odb, progress_cb, progress_payload)) < 0)
		goto cleanup;

	/* Write the data to the ODB */
	{
		foreach_data data = {0};
		data.stats = stats;
		data.progress_cb = progress_cb;
		data.progress_payload = progress_payload;
		data.writepack = writepack;

		if ((error = git_packbuilder_foreach(pack, foreach_cb, &data)) < 0)
			goto cleanup;
	}
	error = writepack->commit(writepack, stats);

cleanup:
	if (writepack) writepack->free(writepack);
	git_packbuilder_free(pack);
	git_revwalk_free(walk);
	return error;
}

static int local_is_connected(git_transport *transport)
{
	transport_local *t = (transport_local *)transport;

	return t->connected;
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

int git_transport_local(git_transport **out, git_remote *owner, void *param)
{
	transport_local *t;

	GIT_UNUSED(param);

	t = git__calloc(1, sizeof(transport_local));
	GITERR_CHECK_ALLOC(t);

	t->parent.version = GIT_TRANSPORT_VERSION;
	t->parent.connect = local_connect;
	t->parent.negotiate_fetch = local_negotiate_fetch;
	t->parent.download_pack = local_download_pack;
	t->parent.close = local_close;
	t->parent.free = local_free;
	t->parent.ls = local_ls;
	t->parent.is_connected = local_is_connected;
	t->parent.read_flags = local_read_flags;
	t->parent.cancel = local_cancel;

	t->owner = owner;

	*out = (git_transport *) t;

	return 0;
}
