/*
 * Copyright (C) 2009-2011 the libgit2 contributors
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
	int i, error;
	const git_commit **parents;

	parents = git__malloc(parent_count * sizeof(git_commit *));

	va_start(ap, parent_count);
	for (i = 0; i < parent_count; ++i)
		parents[i] = va_arg(ap, const git_commit *);
	va_end(ap);

	error = git_commit_create(
		oid, repo, update_ref, author, committer,
		message_encoding, message,
		tree, parent_count, parents);

	git__free((void *)parents);

	return error;
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
	int error, i;

	if (git_object_owner((const git_object *)tree) != repo)
		return git__throw(GIT_EINVALIDARGS, "The given tree does not belong to this repository");

	git_oid__writebuf(&commit, "tree ", git_object_id((const git_object *)tree));

	for (i = 0; i < parent_count; ++i) {
		if (git_object_owner((const git_object *)parents[i]) != repo) {
			error = git__throw(GIT_EINVALIDARGS, "The given parent does not belong to this repository");
			goto cleanup;
		}

		git_oid__writebuf(&commit, "parent ", git_object_id((const git_object *)parents[i]));
	}

	git_signature__writebuf(&commit, "author ", author);
	git_signature__writebuf(&commit, "committer ", committer);

	if (message_encoding != NULL)
		git_buf_printf(&commit, "encoding %s\n", message_encoding);

	git_buf_putc(&commit, '\n');
	git_buf_puts(&commit, message);

	if (git_buf_oom(&commit)) {
		error = git__throw(GIT_ENOMEM, "Not enough memory to build the commit data");
		goto cleanup;
	}

	error = git_odb_write(oid, git_repository_database(repo), commit.ptr, commit.size, GIT_OBJ_COMMIT);
	git_buf_free(&commit);

	if (error == GIT_SUCCESS && update_ref != NULL) {
		git_reference *head;
		git_reference *target;

		error = git_reference_lookup(&head, repo, update_ref);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to create commit");

		error = git_reference_resolve(&target, head);
		if (error < GIT_SUCCESS) {
			if (error != GIT_ENOTFOUND) {
				git_reference_free(head);
				return git__rethrow(error, "Failed to create commit");
			}
		/*
		 * The target of the reference was not found. This can happen
		 * just after a repository has been initialized (the master
		 * branch doesn't exist yet, as it doesn't have anything to
		 * point to) or after an orphan checkout, so if the target
		 * branch doesn't exist yet, create it and return.
		 */
			error = git_reference_create_oid(&target, repo, git_reference_target(head), oid, 1);

			git_reference_free(head);
			if (error == GIT_SUCCESS)
				git_reference_free(target);

			return error;
		}

		error = git_reference_set_oid(target, oid);

		git_reference_free(head);
		git_reference_free(target);
	}

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create commit");

	return GIT_SUCCESS;

cleanup:
	git_buf_free(&commit);
	return error;
}

int git_commit__parse_buffer(git_commit *commit, const void *data, size_t len)
{
	const char *buffer = data;
	const char *buffer_end = (const char *)data + len;

	git_oid parent_oid;
	int error;

	git_vector_init(&commit->parent_oids, 4, NULL);

	if ((error = git_oid__parse(&commit->tree_oid, &buffer, buffer_end, "tree ")) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to parse buffer");

	/*
	 * TODO: commit grafts!
	 */

	while (git_oid__parse(&parent_oid, &buffer, buffer_end, "parent ") == GIT_SUCCESS) {
		git_oid *new_oid;

		new_oid = git__malloc(sizeof(git_oid));
		git_oid_cpy(new_oid, &parent_oid);

		if (git_vector_insert(&commit->parent_oids, new_oid) < GIT_SUCCESS)
			return GIT_ENOMEM;
	}

	commit->author = git__malloc(sizeof(git_signature));
	if ((error = git_signature__parse(commit->author, &buffer, buffer_end, "author ", '\n')) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to parse commit");

	/* Always parse the committer; we need the commit time */
	commit->committer = git__malloc(sizeof(git_signature));
	if ((error = git_signature__parse(commit->committer, &buffer, buffer_end, "committer ", '\n')) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to parse commit");

	if (git__prefixcmp(buffer, "encoding ") == 0) {
		const char *encoding_end;
		buffer += strlen("encoding ");

		encoding_end = buffer;
		while (encoding_end < buffer_end && *encoding_end != '\n')
			encoding_end++;

		commit->message_encoding = git__strndup(buffer, encoding_end - buffer);
		if (!commit->message_encoding)
			return GIT_ENOMEM;

		buffer = encoding_end;
	}

	/* parse commit message */
	while (buffer < buffer_end - 1 && *buffer == '\n')
		buffer++;

	if (buffer <= buffer_end) {
		commit->message = git__strndup(buffer, buffer_end - buffer);
		if (!commit->message)
			return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
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
	if (parent_oid == NULL)
		return git__throw(GIT_ENOTFOUND, "Parent %u does not exist", n);

	return git_commit_lookup(parent, commit->object.repo, parent_oid);
}

const git_oid *git_commit_parent_oid(git_commit *commit, unsigned int n)
{
	assert(commit);

	return git_vector_get(&commit->parent_oids, n);
}
