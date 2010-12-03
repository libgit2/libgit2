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

#include "git/common.h"
#include "git/object.h"
#include "git/repository.h"

#include "common.h"
#include "commit.h"
#include "revwalk.h"
#include "person.h"

#define COMMIT_BASIC_PARSE 0x0
#define COMMIT_FULL_PARSE 0x1

#define COMMIT_PRINT(commit) {\
	char oid[41]; oid[40] = 0;\
	git_oid_fmt(oid, &commit->object.id);\
	printf("Oid: %s | In degree: %d | Time: %u\n", oid, commit->in_degree, commit->commit_time);\
}

static void clear_parents(git_commit *commit)
{
	git_vector_clear(&commit->parents);
}

void git_commit__free(git_commit *commit)
{
	clear_parents(commit);

	git_person__free(commit->author);
	git_person__free(commit->committer);

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

	if (commit->tree == NULL)
		return GIT_EMISSINGOBJDATA;

	git__write_oid(src, "tree", git_tree_id(commit->tree));

	for (i = 0; i < commit->parents.length; ++i) {
		git_commit *parent;

		parent = git_vector_get(&commit->parents, i);
		git__write_oid(src, "parent", git_commit_id(parent));
	}

	if (commit->author == NULL)
		return GIT_EMISSINGOBJDATA;

	git_person__write(src, "author", commit->author);

	if (commit->committer == NULL)
		return GIT_EMISSINGOBJDATA;

	git_person__write(src, "committer", commit->committer);

	if (commit->message != NULL)
		git__source_printf(src, "\n%s", commit->message);

	/* Mark the commit as having all attributes */
	commit->full_parse = 1;

	return GIT_SUCCESS;
}

int commit_parse_buffer(git_commit *commit, void *data, size_t len, unsigned int parse_flags)
{
	char *buffer = (char *)data;
	const char *buffer_end = (char *)data + len;

	git_oid oid;
	int error;

	/* first parse; the vector hasn't been initialized yet */
	if (commit->parents.contents == NULL) {
		git_vector_init(&commit->parents, 4, NULL, NULL);
	}

	clear_parents(commit);


	if ((error = git__parse_oid(&oid, &buffer, buffer_end, "tree ")) < GIT_SUCCESS)
		return error;

	if ((error = git_repository_lookup((git_object **)&commit->tree, commit->object.repo, &oid, GIT_OBJ_TREE)) < GIT_SUCCESS)
		return error;

	/*
	 * TODO: commit grafts!
	 */

	while (git__parse_oid(&oid, &buffer, buffer_end, "parent ") == GIT_SUCCESS) {
		git_commit *parent;

		if ((error = git_repository_lookup((git_object **)&parent, commit->object.repo, &oid, GIT_OBJ_COMMIT)) < GIT_SUCCESS)
			return error;

		if (git_vector_insert(&commit->parents, parent) < GIT_SUCCESS)
			return GIT_ENOMEM;
	}


	if (parse_flags & COMMIT_FULL_PARSE) {
		if (commit->author)
			git_person__free(commit->author);

		commit->author = git__malloc(sizeof(git_person));
		if ((error = git_person__parse(commit->author, &buffer, buffer_end, "author ")) < GIT_SUCCESS)
			return error;

	} else {
		if ((buffer = memchr(buffer, '\n', buffer_end - buffer)) == NULL)
			return GIT_EOBJCORRUPTED;

		buffer++;
	}

	/* Always parse the committer; we need the commit time */
	if (commit->committer)
		git_person__free(commit->committer);

	commit->committer = git__malloc(sizeof(git_person));
	if ((error = git_person__parse(commit->committer, &buffer, buffer_end, "committer ")) < GIT_SUCCESS)
		return error;

	commit->commit_time = commit->committer->time;

	/* parse commit message */
	while (buffer <= buffer_end && *buffer == '\n')
		buffer++;

	if (parse_flags & COMMIT_FULL_PARSE && buffer < buffer_end) {
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
			commit->object.source.raw.data, commit->object.source.raw.len, COMMIT_BASIC_PARSE);
}

int git_commit__parse_full(git_commit *commit)
{
	int error;

	if (commit->full_parse)
		return GIT_SUCCESS;

	if ((error = git_object__source_open((git_object *)commit)) < GIT_SUCCESS)
		return error;

	error = commit_parse_buffer(commit,
			commit->object.source.raw.data, commit->object.source.raw.len, COMMIT_FULL_PARSE);

	git_object__source_close((git_object *)commit);

	commit->full_parse = 1;
	return error;
}



#define GIT_COMMIT_GETTER(_rvalue, _name) \
	const _rvalue git_commit_##_name(git_commit *commit) \
	{\
		assert(commit); \
		if (commit->_name) \
			return commit->_name; \
		if (!commit->object.in_memory) \
			git_commit__parse_full(commit); \
		return commit->_name; \
	}

#define CHECK_FULL_PARSE() \
	if (!commit->object.in_memory && !commit->full_parse)\
		git_commit__parse_full(commit); 

GIT_COMMIT_GETTER(git_tree *, tree)
GIT_COMMIT_GETTER(git_person *, author)
GIT_COMMIT_GETTER(git_person *, committer)
GIT_COMMIT_GETTER(char *, message)
GIT_COMMIT_GETTER(char *, message_short)

time_t git_commit_time(git_commit *commit)
{
	assert(commit);

	if (commit->commit_time)
		return commit->commit_time;

	if (!commit->object.in_memory)
		git_commit__parse_full(commit);

	return commit->commit_time;
}

unsigned int git_commit_parentcount(git_commit *commit)
{
	assert(commit);
	return commit->parents.length;
}

git_commit * git_commit_parent(git_commit *commit, unsigned int n)
{
	assert(commit);
	return git_vector_get(&commit->parents, n);
}

void git_commit_set_tree(git_commit *commit, git_tree *tree)
{
	assert(commit && tree);
	commit->object.modified = 1;
	CHECK_FULL_PARSE();
	commit->tree = tree;
}

void git_commit_set_author(git_commit *commit, const char *name, const char *email, time_t time)
{
	assert(commit && name && email);
	commit->object.modified = 1;
	CHECK_FULL_PARSE();

	git_person__free(commit->author);
	commit->author = git_person__new(name, email, time);
}

void git_commit_set_committer(git_commit *commit, const char *name, const char *email, time_t time)
{
	assert(commit && name && email);
	commit->object.modified = 1;
	CHECK_FULL_PARSE();

	git_person__free(commit->committer);
	commit->committer = git_person__new(name, email, time);
	commit->commit_time = time;
}

void git_commit_set_message(git_commit *commit, const char *message)
{
	const char *short_message;

	commit->object.modified = 1;
	CHECK_FULL_PARSE();

	if (commit->message)
		free(commit->message);

	if (commit->message_short)
		free(commit->message_short);

	commit->message = git__strdup(message);
	commit->message_short = NULL;

	/* TODO: extract short message */
}

int git_commit_add_parent(git_commit *commit, git_commit *new_parent)
{
	CHECK_FULL_PARSE();
	commit->object.modified = 1;
	return git_vector_insert(&commit->parents, new_parent);
}
