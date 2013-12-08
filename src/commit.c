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
#include "git2/sys/commit.h"

#include "common.h"
#include "odb.h"
#include "commit.h"
#include "signature.h"
#include "message.h"

#include <stdarg.h>

void git_commit__free(void *_commit)
{
	git_commit *commit = _commit;

	git_array_clear(commit->parent_ids);

	git_signature_free(commit->author);
	git_signature_free(commit->committer);

	git__free(commit->raw_header);
	git__free(commit->raw_message);
	git__free(commit->message_encoding);
	git__free(commit->summary);

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

int git_commit_create_from_oids(
	git_oid *oid,
	git_repository *repo,
	const char *update_ref,
	const git_signature *author,
	const git_signature *committer,
	const char *message_encoding,
	const char *message,
	const git_oid *tree,
	int parent_count,
	const git_oid *parents[])
{
	git_buf commit = GIT_BUF_INIT;
	int i;
	git_odb *odb;

	assert(oid && repo && tree && parent_count >= 0);

	git_oid__writebuf(&commit, "tree ", tree);

	for (i = 0; i < parent_count; ++i)
		git_oid__writebuf(&commit, "parent ", parents[i]);

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
		return git_reference__update_terminal(repo, update_ref, oid);

	return 0;

on_error:
	git_buf_free(&commit);
	giterr_set(GITERR_OBJECT, "Failed to create commit.");
	return -1;
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
	int retval, i;
	const git_oid **parent_oids;

	assert(parent_count >= 0);
	assert(git_object_owner((const git_object *)tree) == repo);

	parent_oids = git__malloc(parent_count * sizeof(git_oid *));
	GITERR_CHECK_ALLOC(parent_oids);

	for (i = 0; i < parent_count; ++i) {
		assert(git_object_owner((const git_object *)parents[i]) == repo);
		parent_oids[i] = git_object_id((const git_object *)parents[i]);
	}

	retval = git_commit_create_from_oids(
		oid, repo, update_ref, author, committer,
		message_encoding, message,
		git_object_id((const git_object *)tree), parent_count, parent_oids);

	git__free((void *)parent_oids);

	return retval;
}

int git_commit__parse(void *_commit, git_odb_object *odb_obj)
{
	git_commit *commit = _commit;
	const char *buffer_start = git_odb_object_data(odb_obj), *buffer;
	const char *buffer_end = buffer_start + git_odb_object_size(odb_obj);
	git_oid parent_id;
	uint32_t parent_count = 0;
	size_t header_len;

	/* find end-of-header (counting parents as we go) */
	for (buffer = buffer_start; buffer < buffer_end; ++buffer) {
		if (!strncmp("\n\n", buffer, 2)) {
			++buffer;
			break;
		}
		if (!strncmp("\nparent ", buffer, strlen("\nparent ")))
			++parent_count;
	}

	header_len = buffer - buffer_start;
	commit->raw_header = git__strndup(buffer_start, header_len);
	GITERR_CHECK_ALLOC(commit->raw_header);

	/* point "buffer" to header data */
	buffer = commit->raw_header;
	buffer_end = commit->raw_header + header_len;

	if (parent_count < 1)
		parent_count = 1;

	git_array_init_to_size(commit->parent_ids, parent_count);
	GITERR_CHECK_ARRAY(commit->parent_ids);

	if (git_oid__parse(&commit->tree_id, &buffer, buffer_end, "tree ") < 0)
		goto bad_buffer;

	/*
	 * TODO: commit grafts!
	 */

	while (git_oid__parse(&parent_id, &buffer, buffer_end, "parent ") == 0) {
		git_oid *new_id = git_array_alloc(commit->parent_ids);
		GITERR_CHECK_ALLOC(new_id);

		git_oid_cpy(new_id, &parent_id);
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

	/* Parse add'l header entries */
	while (buffer < buffer_end) {
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

	/* point "buffer" to data after header */
	buffer = git_odb_object_data(odb_obj);
	buffer_end = buffer + git_odb_object_size(odb_obj);

	buffer += header_len;
	if (*buffer == '\n')
		++buffer;

	/* extract commit message */
	if (buffer <= buffer_end) {
		commit->raw_message = git__strndup(buffer, buffer_end - buffer);
		GITERR_CHECK_ALLOC(commit->raw_message);
	}

	return 0;

bad_buffer:
	giterr_set(GITERR_OBJECT, "Failed to parse bad commit object");
	return -1;
}

#define GIT_COMMIT_GETTER(_rvalue, _name, _return) \
	_rvalue git_commit_##_name(const git_commit *commit) \
	{\
		assert(commit); \
		return _return; \
	}

GIT_COMMIT_GETTER(const git_signature *, author, commit->author)
GIT_COMMIT_GETTER(const git_signature *, committer, commit->committer)
GIT_COMMIT_GETTER(const char *, message_raw, commit->raw_message)
GIT_COMMIT_GETTER(const char *, message_encoding, commit->message_encoding)
GIT_COMMIT_GETTER(const char *, raw_header, commit->raw_header)
GIT_COMMIT_GETTER(git_time_t, time, commit->committer->when.time)
GIT_COMMIT_GETTER(int, time_offset, commit->committer->when.offset)
GIT_COMMIT_GETTER(unsigned int, parentcount, (unsigned int)git_array_size(commit->parent_ids))
GIT_COMMIT_GETTER(const git_oid *, tree_id, &commit->tree_id);

const char *git_commit_message(const git_commit *commit)
{
	const char *message;

	assert(commit);

	message = commit->raw_message;

	/* trim leading newlines from raw message */
	while (*message && *message == '\n')
		++message;

	return message;
}

const char *git_commit_summary(git_commit *commit)
{
	git_buf summary = GIT_BUF_INIT;
	const char *msg, *space;

	assert(commit);

	if (!commit->summary) {
		for (msg = git_commit_message(commit), space = NULL; *msg; ++msg) {
			if (msg[0] == '\n' && (!msg[1] || msg[1] == '\n'))
				break;
			else if (msg[0] == '\n')
				git_buf_putc(&summary, ' ');
			else if (git__isspace(msg[0]))
				space = space ? space : msg;
			else if (space) {
				git_buf_put(&summary, space, (msg - space) + 1);
				space = NULL;
			} else
				git_buf_putc(&summary, *msg);
		}

		commit->summary = git_buf_detach(&summary);
	}

	return commit->summary;
}

int git_commit_tree(git_tree **tree_out, const git_commit *commit)
{
	assert(commit);
	return git_tree_lookup(tree_out, commit->object.repo, &commit->tree_id);
}

const git_oid *git_commit_parent_id(
	const git_commit *commit, unsigned int n)
{
	assert(commit);

	return git_array_get(commit->parent_ids, n);
}

int git_commit_parent(
	git_commit **parent, const git_commit *commit, unsigned int n)
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
	git_commit *current, *parent = NULL;
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
