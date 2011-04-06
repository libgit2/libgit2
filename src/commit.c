/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
		free(parent);
	}

	git_vector_clear(&commit->parent_oids);
}

void git_commit__free(git_commit *commit)
{
	clear_parents(commit);
	git_vector_free(&commit->parent_oids);

	git_signature_free(commit->author);
	git_signature_free(commit->committer);

	free(commit->message);
	free(commit->message_short);
	free(commit);
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
		const char *message,
		const git_oid *tree_oid,
		int parent_count,
		...)
{
	va_list ap;
	int i, error;
	const git_oid **oids;

	oids = git__malloc(parent_count * sizeof(git_oid *));

	va_start(ap, parent_count);
	for (i = 0; i < parent_count; ++i)
		oids[i] = va_arg(ap, const git_oid *);
	va_end(ap);

	error = git_commit_create(
		oid, repo, update_ref, author, committer, message,
		tree_oid, parent_count, oids);

	free((void *)oids);
	return error;
}

int git_commit_create_ov(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message,
		const git_tree *tree,
		int parent_count,
		...)
{
	va_list ap;
	int i, error;
	const git_oid **oids;

	oids = git__malloc(parent_count * sizeof(git_oid *));

	va_start(ap, parent_count);
	for (i = 0; i < parent_count; ++i)
		oids[i] = git_object_id(va_arg(ap, const git_object *));
	va_end(ap);

	error = git_commit_create(
		oid, repo, update_ref, author, committer, message,
		git_object_id((git_object *)tree),
		parent_count, oids);

	free((void *)oids);
	return error;
}

int git_commit_create_o(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message,
		const git_tree *tree,
		int parent_count,
		const git_commit *parents[])
{
	int i, error;
	const git_oid **oids;

	oids = git__malloc(parent_count * sizeof(git_oid *));

	for (i = 0; i < parent_count; ++i)
		oids[i] = git_object_id((git_object *)parents[i]);

	error = git_commit_create(
		oid, repo, update_ref, author, committer, message,
		git_object_id((git_object *)tree),
		parent_count, oids);
	
	free((void *)oids);
	return error;
}

int git_commit_create(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message,
		const git_oid *tree_oid,
		int parent_count,
		const git_oid *parents[])
{
	size_t final_size = 0;
	int message_length, author_length, committer_length;

	char *author_str, *committer_str;

	int error, i;
	git_odb_stream *stream;

	message_length = strlen(message);
	author_length = git_signature__write(&author_str, "author", author);
	committer_length = git_signature__write(&committer_str, "committer", committer);

	if (author_length < 0 || committer_length < 0)
		return GIT_ENOMEM;

	final_size += GIT_OID_LINE_LENGTH("tree");
	final_size += GIT_OID_LINE_LENGTH("parent") * parent_count;
	final_size += author_length;
	final_size += committer_length;
	final_size += 1 + message_length;

	if ((error = git_odb_open_wstream(&stream, repo->db, final_size, GIT_OBJ_COMMIT)) < GIT_SUCCESS)
		return error;

	git__write_oid(stream, "tree", tree_oid);

	for (i = 0; i < parent_count; ++i)
		git__write_oid(stream, "parent", parents[i]);

	stream->write(stream, author_str, author_length);
	free(author_str);

	stream->write(stream, committer_str, committer_length);
	free(committer_str);


	stream->write(stream, "\n", 1);
	stream->write(stream, message, message_length);

	error = stream->finalize_write(oid, stream);
	stream->free(stream);

	if (error == GIT_SUCCESS && update_ref != NULL) {
		git_reference *head;

		error = git_reference_lookup(&head, repo, update_ref);
		if (error < GIT_SUCCESS)
			return error;

		if (git_reference_type(head) == GIT_REF_SYMBOLIC) {
			if ((error = git_reference_resolve(&head, head)) < GIT_SUCCESS)
				return error;
		}

		error = git_reference_set_oid(head, oid);
	}

	return error;
}

int commit_parse_buffer(git_commit *commit, const void *data, size_t len)
{
	const char *buffer = (char *)data;
	const char *buffer_end = (char *)data + len;

	git_oid parent_oid;
	int error;

	git_vector_init(&commit->parent_oids, 4, NULL);

	if ((error = git__parse_oid(&commit->tree_oid, &buffer, buffer_end, "tree ")) < GIT_SUCCESS)
		return error;

	/*
	 * TODO: commit grafts!
	 */

	while (git__parse_oid(&parent_oid, &buffer, buffer_end, "parent ") == GIT_SUCCESS) {
		git_oid *new_oid;

		new_oid = git__malloc(sizeof(git_oid));
		git_oid_cpy(new_oid, &parent_oid);

		if (git_vector_insert(&commit->parent_oids, new_oid) < GIT_SUCCESS)
			return GIT_ENOMEM;
	}

	commit->author = git__malloc(sizeof(git_signature));
	if ((error = git_signature__parse(commit->author, &buffer, buffer_end, "author ")) < GIT_SUCCESS)
		return error;

	/* Always parse the committer; we need the commit time */
	commit->committer = git__malloc(sizeof(git_signature));
	if ((error = git_signature__parse(commit->committer, &buffer, buffer_end, "committer ")) < GIT_SUCCESS)
		return error;

	/* parse commit message */
	while (buffer <= buffer_end && *buffer == '\n')
		buffer++;

	if (buffer < buffer_end) {
		const char *line_end;
		size_t message_len = buffer_end - buffer;

		/* Long message */
		message_len = buffer_end - buffer;
		commit->message = git__malloc(message_len + 1);
		memcpy(commit->message, buffer, message_len);
		commit->message[message_len] = 0;

		/* Short message */
		if((line_end = memchr(buffer, '\n', buffer_end - buffer)) == NULL)
			line_end = buffer_end;
		message_len = line_end - buffer;

		commit->message_short = git__malloc(message_len + 1);
		memcpy(commit->message_short, buffer, message_len);
		commit->message_short[message_len] = 0;
	}

	return GIT_SUCCESS;
}

int git_commit__parse(git_commit *commit, git_odb_object *obj)
{
	assert(commit);
	return commit_parse_buffer(commit, obj->raw.data, obj->raw.len);
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
GIT_COMMIT_GETTER(const char *, message_short, commit->message_short)
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
		return GIT_ENOTFOUND;

	return git_commit_lookup(parent, commit->object.repo, parent_oid);
}

const git_oid *git_commit_parent_oid(git_commit *commit, unsigned int n)
{
	assert(commit);

	return git_vector_get(&commit->parent_oids, n);
}
