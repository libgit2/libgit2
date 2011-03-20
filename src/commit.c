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
#include "commit.h"
#include "revwalk.h"
#include "signature.h"

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

int git_commit__writeback(git_commit *commit, git_odb_source *src)
{
	unsigned int i;

	git__write_oid(src, "tree", &commit->tree_oid);

	for (i = 0; i < commit->parent_oids.length; ++i) {
		git_oid *parent_oid;

		parent_oid = git_vector_get(&commit->parent_oids, i);
		git__write_oid(src, "parent", parent_oid);
	}

	if (commit->author == NULL)
		return GIT_EMISSINGOBJDATA;

	git_signature__write(src, "author", commit->author);

	if (commit->committer == NULL)
		return GIT_EMISSINGOBJDATA;

	git_signature__write(src, "committer", commit->committer);

	if (commit->message != NULL) {
		git__source_write(src, "\n", 1);
		git__source_write(src, commit->message, strlen(commit->message));
	}

	return GIT_SUCCESS;
}

int commit_parse_buffer(git_commit *commit, void *data, size_t len)
{
	char *buffer = (char *)data;
	const char *buffer_end = (char *)data + len;

	git_oid parent_oid;
	int error;

	/* first parse; the vector hasn't been initialized yet */
	if (commit->parent_oids.contents == NULL) {
		git_vector_init(&commit->parent_oids, 4, NULL);
	}

	clear_parents(commit);

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

	if (commit->author)
		git_signature_free(commit->author);

	commit->author = git__malloc(sizeof(git_signature));
	if ((error = git_signature__parse(commit->author, &buffer, buffer_end, "author ")) < GIT_SUCCESS)
		return error;

	/* Always parse the committer; we need the commit time */
	if (commit->committer)
		git_signature_free(commit->committer);

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

int git_commit__parse(git_commit *commit)
{
	assert(commit && commit->object.source.open);
	return commit_parse_buffer(commit,
			commit->object.source.raw.data, commit->object.source.raw.len);
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



int git_commit_set_tree(git_commit *commit, git_tree *tree)
{
	const git_oid *oid;

	assert(commit && tree);

	if ((oid = git_object_id((git_object *)tree)) == NULL)
		return GIT_EMISSINGOBJDATA;

	commit->object.modified = 1;
	git_oid_cpy(&commit->tree_oid, oid);
	return GIT_SUCCESS;
}

int git_commit_add_parent(git_commit *commit, git_commit *new_parent)
{
	const git_oid *parent_oid;
	git_oid *new_oid;
	assert(commit && new_parent);

	if ((parent_oid = git_object_id((git_object *)new_parent)) == NULL)
		return GIT_EMISSINGOBJDATA;

	new_oid = git__malloc(sizeof(git_oid));
	if (new_oid == NULL)
		return GIT_ENOMEM;

	commit->object.modified = 1;
	git_oid_cpy(new_oid, parent_oid);
	return git_vector_insert(&commit->parent_oids, new_oid);
}

void git_commit_set_author(git_commit *commit, const git_signature *author_sig)
{
	assert(commit && author_sig);
	commit->object.modified = 1;

	git_signature_free(commit->author);
	commit->author = git_signature_dup(author_sig);
}

void git_commit_set_committer(git_commit *commit, const git_signature *committer_sig)
{
	assert(commit && committer_sig);
	commit->object.modified = 1;

	git_signature_free(commit->committer);
	commit->committer = git_signature_dup(committer_sig);
}

void git_commit_set_message(git_commit *commit, const char *message)
{
	const char *line_end;
	size_t message_len;

	commit->object.modified = 1;

	if (commit->message)
		free(commit->message);

	if (commit->message_short)
		free(commit->message_short);

	commit->message = git__strdup(message);

	/* Short message */
	if((line_end = strchr(message, '\n')) == NULL) {
		commit->message_short = git__strdup(message);
		return;
	}

	message_len = line_end - message;

	commit->message_short = git__malloc(message_len + 1);
	memcpy(commit->message_short, message, message_len);
	commit->message_short[message_len] = 0;
}

