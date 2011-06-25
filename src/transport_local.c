#include "common.h"
#include "git2/types.h"
#include "git2/transport.h"
#include "git2/net.h"
#include "git2/repository.h"
#include "git2/object.h"
#include "git2/tag.h"
#include "refs.h"
#include "transport.h"

typedef struct {
	git_vector *vec;
	git_repository *repo;
} callback_data;

static int compare_heads(const void *a, const void *b)
{
	const git_remote_head *heada = *(const git_remote_head **)a;
	const git_remote_head *headb = *(const git_remote_head **)b;

	return strcmp(heada->name, headb->name);
}

/*
 * Try to open the url as a git directory. The direction doesn't
 * matter in this case because we're calulating the heads ourselves.
 */
static int local_connect(git_transport *transport, git_net_direction GIT_UNUSED(dir))
{
	git_repository *repo;
	int error;
	const char *path;
	const char file_prefix[] = "file://";
	GIT_UNUSED_ARG(dir);

	/* The repo layer doesn't want the prefix */
	if (!git__prefixcmp(transport->url, file_prefix))
		path = transport->url + STRLEN(file_prefix);
	else
		path = transport->url;

	error = git_repository_open(&repo, path);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to open remote");

	transport->private = repo;

	transport->connected = 1;

	return GIT_SUCCESS;
}

static int heads_cb(const char *name, void *ptr)
{
	callback_data *data = ptr;
	git_vector *vec = data->vec;
	git_repository *repo = data->repo;
	const char peeled[] = "^{}";
	git_remote_head *head;
	git_reference *ref;
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

	error = git_reference_resolve(&ref, ref);
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
	peel_len = strlen(name) + STRLEN(peeled);
	head->name = git__malloc(peel_len + 1);
	ret = snprintf(head->name, peel_len + 1, "%s%s", name, peeled);
	if (ret >= peel_len + 1) {
		error = git__throw(GIT_ERROR, "The string is magically to long");
	}

	git_oid_cpy(&head->oid, git_tag_target_oid((git_tag *) obj));

	error = git_vector_insert(vec, head);
	if (error < GIT_SUCCESS)
		goto out;

 out:
	git_object_close(obj);
	if (error < GIT_SUCCESS) {
		free(head->name);
		free(head);
	}
	return error;
}

static int local_ls(git_transport *transport, git_headarray *array)
{
	int error;
	git_repository *repo;
	git_vector vec;
	callback_data data;

	assert(transport && transport->connected);

	repo = transport->private;
	error = git_vector_init(&vec, 16, compare_heads);
	if (error < GIT_SUCCESS)
		return error;

	data.vec = &vec;
	data.repo = repo;
	error = git_reference_foreach(repo, GIT_REF_LISTALL, heads_cb, &data);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to list remote heads");

	git_vector_sort(&vec);
	array->len = vec.length;
	array->heads = (git_remote_head **) vec.contents;

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
	assert(transport);

	git_repository_free(transport->private);
	free(transport->url);
	free(transport);
}

/**************
 * Public API *
 **************/

int git_transport_local(git_transport *transport)
{
	transport->connect = local_connect;
	transport->ls = local_ls;
	transport->close = local_close;
	transport->free = local_free;

	return GIT_SUCCESS;
}
