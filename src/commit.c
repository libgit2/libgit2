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
#include "git/odb.h"
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
	git_commit_parents *node, *next_node;

	node = commit->parents;
	while (node) {
		next_node = node->next;
		free(node);
		node = next_node;
	}

	commit->parents = NULL;
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
	git_commit_parents *parent;

	if (commit->tree == NULL)
		return GIT_EMISSINGOBJDATA;

	git__write_oid(src, "tree", git_tree_id(commit->tree));

	parent = commit->parents;

	while (parent != NULL) {
		git__write_oid(src, "parent", git_commit_id(parent->commit));
		parent = parent->next;
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

	if ((error = git__parse_oid(&oid, &buffer, buffer_end, "tree ")) < 0)
		return error;

	if ((error = git_repository_lookup((git_object **)&commit->tree, commit->object.repo, &oid, GIT_OBJ_TREE)) < 0)
		return error;

	/*
	 * TODO: commit grafts!
	 */

	clear_parents(commit);

	while (git__parse_oid(&oid, &buffer, buffer_end, "parent ") == 0) {
		git_commit *parent;
		git_commit_parents *node;

		if ((error = git_repository_lookup((git_object **)&parent, commit->object.repo, &oid, GIT_OBJ_COMMIT)) < 0)
			return error;

		if ((node = git__malloc(sizeof(git_commit_parents))) == NULL)
			return GIT_ENOMEM;

		node->commit = parent;
		node->next = commit->parents;
		commit->parents = node;
	}


	if (parse_flags & COMMIT_FULL_PARSE) {
		if (commit->author)
			git_person__free(commit->author);

		commit->author = git__malloc(sizeof(git_person));
		if ((error = git_person__parse(commit->author, &buffer, buffer_end, "author ")) < 0)
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
	if ((error = git_person__parse(commit->committer, &buffer, buffer_end, "committer ")) < 0)
		return error;

	commit->commit_time = commit->committer->time;

	/* parse commit message */
	while (buffer <= buffer_end && *buffer == '\n')
		buffer++;

	if (parse_flags & COMMIT_FULL_PARSE && buffer < buffer_end) {
		char *line_end;
		size_t message_len = buffer_end - buffer;

		/* Short message */
		message_len = buffer_end - buffer;
		commit->message = git__malloc(message_len + 1);
		memcpy(commit->message, buffer, message_len);
		commit->message[message_len] = 0;

		/* Long message */
		line_end = memchr(buffer, '\n', buffer_end - buffer);
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
		return 0;

	if ((error = git_object__source_open((git_object *)commit)) < 0)
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

void git_commit_add_parent(git_commit *commit, git_commit *new_parent)
{
	git_commit_parents *node;

	commit->object.modified = 1;
	CHECK_FULL_PARSE();

	if ((node = git__malloc(sizeof(git_commit_parents))) == NULL)
		return;

	node->commit = new_parent;
	node->next = commit->parents;
	commit->parents = node;
}
