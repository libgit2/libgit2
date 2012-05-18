/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/common.h"
#include "git2/object.h"
#include "git2/repository.h"
#include "git2/signature.h"

#include "common.h"
#include "odb.h"
#include "commit.h"
#include "signature.h"
#include "message.h"

#include <stdarg.h>

#define COMMIT_BASIC_PARSE 0x0
#define COMMIT_FULL_PARSE 0x1

#define COMMIT_PRINT(commit) {\
	char oid[41]; oid[40] = 0;\
	git_oid_fmt(oid, &commit->object.id);\
	printf("Oid: %s | In degree: %d | Time: %u\n", oid, commit->in_degree, commit->commit_time);\
}

static void clear_parents(git_commit *commit)
{
	unsigned int i;

	for (i = 0; i < commit->parent_oids.length; ++i) {
		git_oid *parent = git_vector_get(&commit->parent_oids, i);
		git__free(parent);
	}

	git_vector_clear(&commit->parent_oids);
}

void git_commit__free(git_commit *commit)
{
	clear_parents(commit);
	git_vector_free(&commit->parent_oids);

	git_signature_free(commit->author);
	git_signature_free(commit->committer);

	git__free(commit->message);
	git__free(commit->message_encoding);
	git__free(commit);
}

const git_oid *git_commit_id(git_commit *c)
{
	return git_object_id((git_object *)c);
}

int git_commit_create_v(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message_encoding,
		const char *message,
		const git_tree *tree,
		int parent_count,
		...)
{
	va_list ap;
	int i, res;
	const git_commit **parents;

	parents = git__malloc(parent_count * sizeof(git_commit *));
	GITERR_CHECK_ALLOC(parents);

	va_start(ap, parent_count);
	for (i = 0; i < parent_count; ++i)
		parents[i] = va_arg(ap, const git_commit *);
	va_end(ap);

	res = git_commit_create(
		oid, repo, update_ref, author, committer,
		message_encoding, message,
		tree, parent_count, parents);

	git__free((void *)parents);
	return res;
}

/* Update the reference named `ref_name` so it points to `oid` */
static int update_reference(git_repository *repo, git_oid *oid, const char *ref_name)
{
	git_reference *ref;
	int res;

	res = git_reference_lookup(&ref, repo, ref_name);

	/* If we haven't found the reference at all, we assume we need to create
	 * a new reference and that's it */
	if (res == GIT_ENOTFOUND) {
		giterr_clear();
		return git_reference_create_oid(NULL, repo, ref_name, oid, 1);
	}

	if (res < 0)
		return -1;

	/* If we have found a reference, but it's symbolic, we need to update
	 * the direct reference it points to */
	if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
		git_reference *aux;
		const char *sym_target;

		/* The target pointed at by this reference */
		sym_target = git_reference_target(ref);

		/* resolve the reference to the target it points to */
		res = git_reference_resolve(&aux, ref);

		/*
		 * if the symbolic reference pointed to an inexisting ref,
		 * this is means we're creating a new branch, for example.
		 * We need to create a new direct reference with that name
		 */
		if (res == GIT_ENOTFOUND) {
			giterr_clear();
			res = git_reference_create_oid(NULL, repo, sym_target, oid, 1);
			git_reference_free(ref);
			return res;
		}

		/* free the original symbolic reference now; not before because
		 * we're using the `sym_target` pointer */
		git_reference_free(ref);

		if (res < 0)
			return -1;

		/* store the newly found direct reference in its place */
		ref = aux;
	}

	/* ref is made to point to `oid`: ref is either the original reference,
	 * or the target of the symbolic reference we've looked up */
	res = git_reference_set_oid(ref, oid);
	git_reference_free(ref);
	return res;
}

int git_commit_create(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message_encoding,
		const char *message,
		const git_tree *tree,
		int parent_count,
		const git_commit *parents[])
{
	git_buf commit = GIT_BUF_INIT, cleaned_message = GIT_BUF_INIT;
	int i;
	git_odb *odb;

	assert(git_object_owner((const git_object *)tree) == repo);

	git_oid__writebuf(&commit, "tree ", git_object_id((const git_object *)tree));

	for (i = 0; i < parent_count; ++i) {
		assert(git_object_owner((const git_object *)parents[i]) == repo);
		git_oid__writebuf(&commit, "parent ", git_object_id((const git_object *)parents[i]));
	}

	git_signature__writebuf(&commit, "author ", author);
	git_signature__writebuf(&commit, "committer ", committer);

	if (message_encoding != NULL)
		git_buf_printf(&commit, "encoding %s\n", message_encoding);

	git_buf_putc(&commit, '\n');

	/* Remove comments by default */
	if (git_message_prettify(&cleaned_message, message, 1) < 0)
		goto on_error;

	if (git_buf_puts(&commit, git_buf_cstr(&cleaned_message)) < 0)
		goto on_error;

	git_buf_free(&cleaned_message);

	if (git_repository_odb__weakptr(&odb, repo) < 0)
		goto on_error;

	if (git_odb_write(oid, odb, commit.ptr, commit.size, GIT_OBJ_COMMIT) < 0)
		goto on_error;

	git_buf_free(&commit);

	if (update_ref != NULL)
		return update_reference(repo, oid, update_ref);

	return 0;

on_error:
	git_buf_free(&commit);
	git_buf_free(&cleaned_message);
	giterr_set(GITERR_OBJECT, "Failed to create commit.");
	return -1;
}

int git_commit__parse_buffer(git_commit *commit, const void *data, size_t len)
{
	const char *buffer = data;
	const char *buffer_end = (const char *)data + len;

	git_oid parent_oid;

	git_vector_init(&commit->parent_oids, 4, NULL);

	if (git_oid__parse(&commit->tree_oid, &buffer, buffer_end, "tree ") < 0)
		goto bad_buffer;

	/*
	 * TODO: commit grafts!
	 */

	while (git_oid__parse(&parent_oid, &buffer, buffer_end, "parent ") == 0) {
		git_oid *new_oid;

		new_oid = git__malloc(sizeof(git_oid));
		GITERR_CHECK_ALLOC(new_oid);

		git_oid_cpy(new_oid, &parent_oid);

		if (git_vector_insert(&commit->parent_oids, new_oid) < 0)
			return -1;
	}

	commit->author = git__malloc(sizeof(git_signature));
	GITERR_CHECK_ALLOC(commit->author);

	if (git_signature__parse(commit->author, &buffer, buffer_end, "author ", '\n') < 0)
		return -1;

	/* Always parse the committer; we need the commit time */
	commit->committer = git__malloc(sizeof(git_signature));
	GITERR_CHECK_ALLOC(commit->committer);

	if (git_signature__parse(commit->committer, &buffer, buffer_end, "committer ", '\n') < 0)
		return -1;

	if (git__prefixcmp(buffer, "encoding ") == 0) {
		const char *encoding_end;
		buffer += strlen("encoding ");

		encoding_end = buffer;
		while (encoding_end < buffer_end && *encoding_end != '\n')
			encoding_end++;

		commit->message_encoding = git__strndup(buffer, encoding_end - buffer);
		GITERR_CHECK_ALLOC(commit->message_encoding);

		buffer = encoding_end;
	}

	/* parse commit message */
	while (buffer < buffer_end - 1 && *buffer == '\n')
		buffer++;

	if (buffer <= buffer_end) {
		commit->message = git__strndup(buffer, buffer_end - buffer);
		GITERR_CHECK_ALLOC(commit->message);
	}

	return 0;

bad_buffer:
	giterr_set(GITERR_OBJECT, "Failed to parse bad commit object");
	return -1;
}

int git_commit__parse(git_commit *commit, git_odb_object *obj)
{
	assert(commit);
	return git_commit__parse_buffer(commit, obj->raw.data, obj->raw.len);
}

#define GIT_COMMIT_GETTER(_rvalue, _name, _return) \
	_rvalue git_commit_##_name(git_commit *commit) \
	{\
		assert(commit); \
		return _return; \
	}

GIT_COMMIT_GETTER(const git_signature *, author, commit->author)
GIT_COMMIT_GETTER(const git_signature *, committer, commit->committer)
GIT_COMMIT_GETTER(const char *, message, commit->message)
GIT_COMMIT_GETTER(const char *, message_encoding, commit->message_encoding)
GIT_COMMIT_GETTER(git_time_t, time, commit->committer->when.time)
GIT_COMMIT_GETTER(int, time_offset, commit->committer->when.offset)
GIT_COMMIT_GETTER(unsigned int, parentcount, commit->parent_oids.length)
GIT_COMMIT_GETTER(const git_oid *, tree_oid, &commit->tree_oid);


int git_commit_tree(git_tree **tree_out, git_commit *commit)
{
	assert(commit);
	return git_tree_lookup(tree_out, commit->object.repo, &commit->tree_oid);
}

int git_commit_parent(git_commit **parent, git_commit *commit, unsigned int n)
{
	git_oid *parent_oid;
	assert(commit);

	parent_oid = git_vector_get(&commit->parent_oids, n);
	if (parent_oid == NULL) {
		giterr_set(GITERR_INVALID, "Parent %u does not exist", n);
		return GIT_ENOTFOUND;
	}

	return git_commit_lookup(parent, commit->object.repo, parent_oid);
}

const git_oid *git_commit_parent_oid(git_commit *commit, unsigned int n)
{
	assert(commit);

	return git_vector_get(&commit->parent_oids, n);
}
