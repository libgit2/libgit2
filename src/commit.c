/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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

static void clear_parents(git_commit *commit)
{
	unsigned int i;

	for (i = 0; i < commit->parent_ids.length; ++i) {
		git_oid *parent = git_vector_get(&commit->parent_ids, i);
		git__free(parent);
	}

	git_vector_clear(&commit->parent_ids);
}

void git_commit__free(git_commit *commit)
{
	clear_parents(commit);
	git_vector_free(&commit->parent_ids);

	git_signature_free(commit->author);
	git_signature_free(commit->committer);

	git__free(commit->message);
	git__free(commit->message_encoding);
	git__free(commit);
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
	git_buf commit = GIT_BUF_INIT;
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

	if (git_buf_puts(&commit, message) < 0)
		goto on_error;

	if (git_repository_odb__weakptr(&odb, repo) < 0)
		goto on_error;

	if (git_odb_write(oid, odb, commit.ptr, commit.size, GIT_OBJ_COMMIT) < 0)
		goto on_error;

	git_buf_free(&commit);

	if (update_ref != NULL)
		return git_reference__update(repo, oid, update_ref);

	return 0;

on_error:
	git_buf_free(&commit);
	giterr_set(GITERR_OBJECT, "Failed to create commit.");
	return -1;
}

int git_commit__parse_buffer(git_commit *commit, const void *data, size_t len)
{
	const char *buffer = data;
	const char *buffer_end = (const char *)data + len;
	git_oid parent_id;

	if (git_vector_init(&commit->parent_ids, 4, NULL) < 0)
		return -1;

	if (git_oid__parse(&commit->tree_id, &buffer, buffer_end, "tree ") < 0)
		goto bad_buffer;

	/*
	 * TODO: commit grafts!
	 */

	while (git_oid__parse(&parent_id, &buffer, buffer_end, "parent ") == 0) {
		git_oid *new_id = git__malloc(sizeof(git_oid));
		GITERR_CHECK_ALLOC(new_id);

		git_oid_cpy(new_id, &parent_id);

		if (git_vector_insert(&commit->parent_ids, new_id) < 0)
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

	/* Parse add'l header entries until blank line found */
	while (buffer < buffer_end && *buffer != '\n') {
		const char *eoln = buffer;
		while (eoln < buffer_end && *eoln != '\n')
			++eoln;

		if (git__prefixcmp(buffer, "encoding ") == 0) {
			buffer += strlen("encoding ");

			commit->message_encoding = git__strndup(buffer, eoln - buffer);
			GITERR_CHECK_ALLOC(commit->message_encoding);
		}

		if (eoln < buffer_end && *eoln == '\n')
			++eoln;

		buffer = eoln;
	}

	/* skip blank lines */
	while (buffer < buffer_end - 1 && *buffer == '\n')
		buffer++;

	/* parse commit message */
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
	_rvalue git_commit_##_name(const git_commit *commit) \
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
GIT_COMMIT_GETTER(unsigned int, parentcount, (unsigned int)commit->parent_ids.length)
GIT_COMMIT_GETTER(const git_oid *, tree_id, &commit->tree_id);

int git_commit_tree(git_tree **tree_out, const git_commit *commit)
{
	assert(commit);
	return git_tree_lookup(tree_out, commit->object.repo, &commit->tree_id);
}

const git_oid *git_commit_parent_id(git_commit *commit, unsigned int n)
{
	assert(commit);

	return git_vector_get(&commit->parent_ids, n);
}

int git_commit_parent(git_commit **parent, git_commit *commit, unsigned int n)
{
	const git_oid *parent_id;
	assert(commit);

	parent_id = git_commit_parent_id(commit, n);
	if (parent_id == NULL) {
		giterr_set(GITERR_INVALID, "Parent %u does not exist", n);
		return GIT_ENOTFOUND;
	}

	return git_commit_lookup(parent, commit->object.repo, parent_id);
}

int git_commit_nth_gen_ancestor(
	git_commit **ancestor,
	const git_commit *commit,
	unsigned int n)
{
	git_commit *current, *parent;
	int error;

	assert(ancestor && commit);

	current = (git_commit *)commit;

	if (n == 0)
		return git_commit_lookup(
			ancestor,
			commit->object.repo,
			git_object_id((const git_object *)commit));

	while (n--) {
		error = git_commit_parent(&parent, (git_commit *)current, 0);

		if (current != commit)
			git_commit_free(current);

		if (error < 0)
			return error;

		current = parent;
	}

	*ancestor = parent;
	return 0;
}
