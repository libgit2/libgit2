/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/types.h"
#include "git2/transport.h"
#include "git2/net.h"
#include "git2/repository.h"
#include "git2/object.h"
#include "git2/tag.h"
#include "refs.h"
#include "transport.h"
#include "posix.h"

typedef struct {
	git_transport parent;
	git_repository *repo;
	git_vector *refs;
} transport_local;

/*
 * Try to open the url as a git directory. The direction doesn't
 * matter in this case because we're calulating the heads ourselves.
 */
static int local_connect(git_transport *transport, int GIT_UNUSED(direction))
{
	git_repository *repo;
	int error;
	transport_local *t = (transport_local *) transport;
	const char *path;
	const char file_prefix[] = "file://";
	GIT_UNUSED_ARG(direction);

	/* The repo layer doesn't want the prefix */
	if (!git__prefixcmp(transport->url, file_prefix))
		path = transport->url + strlen(file_prefix);
	else
		path = transport->url;

	error = git_repository_open(&repo, path);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to open remote");

	t->repo = repo;
	t->parent.connected = 1;

	return GIT_SUCCESS;
}

static int add_ref(const char *name, git_repository *repo, git_vector *vec)
{
	const char peeled[] = "^{}";
	git_remote_head *head;
	git_reference *ref, *resolved_ref;
	git_object *obj = NULL;
	int error = GIT_SUCCESS, peel_len, ret;

	head = git__malloc(sizeof(git_remote_head));
	if (head == NULL)
		return GIT_ENOMEM;

	head->name = git__strdup(name);
	if (head->name == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	error = git_reference_lookup(&ref, repo, name);
	if (error < GIT_SUCCESS)
		goto out;

	error = git_reference_resolve(&resolved_ref, ref);
	if (error < GIT_SUCCESS)
		goto out;

	git_oid_cpy(&head->oid, git_reference_oid(ref));

	error = git_vector_insert(vec, head);
	if (error < GIT_SUCCESS)
		goto out;

	/* If it's not a tag, we don't need to try to peel it */
	if (git__prefixcmp(name, GIT_REFS_TAGS_DIR))
		goto out;

	error = git_object_lookup(&obj, repo, &head->oid, GIT_OBJ_ANY);
	if (error < GIT_SUCCESS) {
		git__rethrow(error, "Failed to lookup object");
	}

	/* If it's not an annotated tag, just get out */
	if (git_object_type(obj) != GIT_OBJ_TAG)
		goto out;

	/* And if it's a tag, peel it, and add it to the list */
	head = git__malloc(sizeof(git_remote_head));
	peel_len = strlen(name) + strlen(peeled);
	head->name = git__malloc(peel_len + 1);
	ret = p_snprintf(head->name, peel_len + 1, "%s%s", name, peeled);
	if (ret >= peel_len + 1) {
		error = git__throw(GIT_ERROR, "The string is magically to long");
	}

	git_oid_cpy(&head->oid, git_tag_target_oid((git_tag *) obj));

	error = git_vector_insert(vec, head);
	if (error < GIT_SUCCESS)
		goto out;

 out:
	git_reference_free(ref);
	git_reference_free(resolved_ref);

	git_object_close(obj);
	if (error < GIT_SUCCESS) {
		git__free(head->name);
		git__free(head);
	}
	return error;
}

static int local_ls(git_transport *transport, git_headarray *array)
{
	int error;
	unsigned int i;
	git_repository *repo;
	git_vector *vec;
	git_strarray refs;
	transport_local *t = (transport_local *) transport;

	assert(transport && transport->connected);

	repo = t->repo;

	error = git_reference_listall(&refs, repo, GIT_REF_LISTALL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to list remote heads");

	vec = git__malloc(sizeof(git_vector));
	if (vec == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	error = git_vector_init(vec, refs.count, NULL);
	if (error < GIT_SUCCESS)
		return error;

	/* Sort the references first */
	git__tsort((void **)refs.strings, refs.count, &git__strcmp_cb);

	/* Add HEAD */
	error = add_ref(GIT_HEAD_FILE, repo, vec);
	if (error < GIT_SUCCESS)
		goto out;

	for (i = 0; i < refs.count; ++i) {
		error = add_ref(refs.strings[i], repo, vec);
		if (error < GIT_SUCCESS)
			goto out;
	}

	array->len = vec->length;
	array->heads = (git_remote_head **)vec->contents;

	t->refs = vec;

 out:

	git_strarray_free(&refs);

	return error;
}

static int local_close(git_transport *GIT_UNUSED(transport))
{
	/* Nothing to do */
	GIT_UNUSED_ARG(transport);
	return GIT_SUCCESS;
}

static void local_free(git_transport *transport)
{
	unsigned int i;
	transport_local *t = (transport_local *) transport;
	git_vector *vec = t->refs;
	git_remote_head *h;

	assert(transport);

	if (t->refs != NULL) {
		git_vector_foreach (vec, i, h) {
			git__free(h->name);
			git__free(h);
		}
		git_vector_free(vec);
		git__free(vec);
	}

	git_repository_free(t->repo);
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
	if (t == NULL)
		return GIT_ENOMEM;

	memset(t, 0x0, sizeof(transport_local));

	t->parent.connect = local_connect;
	t->parent.ls = local_ls;
	t->parent.close = local_close;
	t->parent.free = local_free;

	*out = (git_transport *) t;

	return GIT_SUCCESS;
}
