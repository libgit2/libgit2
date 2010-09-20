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

#include "common.h"
#include "commit.h"
#include "revwalk.h"
#include "git/odb.h"
#include "git/repository.h"

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

	free(commit->author);
	free(commit->committer);
	free(commit->message);
	free(commit->message_short);
	free(commit);
}

git_commit *git_commit_new(git_repository *repo)
{
	return (git_commit *)git_object_new(repo, GIT_OBJ_COMMIT);
}

const git_oid *git_commit_id(git_commit *c)
{
	return git_object_id((git_object *)c);
}

int git_commit__parse(git_commit *commit)
{
	const int close_db_object = 1;
	int error = 0;

	if ((error = git_object__source_open((git_object *)commit)) < 0)
		return error;

	error = git_commit__parse_buffer(commit,
			commit->object.source.raw.data, commit->object.source.raw.len, COMMIT_BASIC_PARSE);

	if (close_db_object)
		git_object__source_close((git_object *)commit);

	return error;
}

int git_commit__parse_full(git_commit *commit)
{
	int error;

	if (commit->full_parse)
		return 0;

	if (git_object__source_open((git_object *)commit) < 0)
		return GIT_ERROR;

	error = git_commit__parse_buffer(commit,
			commit->object.source.raw.data, commit->object.source.raw.len, COMMIT_FULL_PARSE);

	git_object__source_close((git_object *)commit);

	commit->full_parse = 1;
	return error;
}

git_commit *git_commit_lookup(git_repository *repo, const git_oid *id)
{
	return (git_commit *)git_repository_lookup(repo, id, GIT_OBJ_COMMIT);
}

int git__parse_person(git_person *person, char **buffer_out,
		const char *buffer_end, const char *header)
{
	const size_t header_len = strlen(header);

	int i;
	char *buffer = *buffer_out;
	char *line_end, *name, *email;

	line_end = memchr(buffer, '\n', buffer_end - buffer);
	if (!line_end)
		return GIT_EOBJCORRUPTED;

	if (buffer + (header_len + 1) > line_end)
		return GIT_EOBJCORRUPTED;

	if (memcmp(buffer, header, header_len) != 0)
		return GIT_EOBJCORRUPTED;

	buffer += header_len;


	/* Parse name field */
	for (i = 0, name = person->name;
		 i < 64 && buffer < line_end && *buffer != '<';
		 ++i)
		*name++ = *buffer++;

	*(name - 1) = 0;

	while (buffer < line_end && *buffer != '<')
		buffer++;

	if (++buffer >= line_end)
		return GIT_EOBJCORRUPTED;

	/* Parse email field */
	for (i = 0, email = person->email;
		 i < 64 && buffer < line_end && *buffer != '>';
		 ++i)
		*email++ = *buffer++;

	*email = 0;

	while (buffer < line_end && *buffer != '>')
		buffer++;

	if (++buffer >= line_end)
		return GIT_EOBJCORRUPTED;

	person->time = strtol(buffer, &buffer, 10);

	if (person->time == 0)
		return GIT_EOBJCORRUPTED;

	*buffer_out = (line_end + 1);
	return 0;
}

int git__write_person(git_odb_source *src, const char *header, const git_person *person)
{
	return git__source_printf(src, "%s %s <%s> %u\n", header, person->name, person->email, person->time);
}

int git__parse_oid(git_oid *oid, char **buffer_out,
		const char *buffer_end, const char *header)
{
	const size_t sha_len = GIT_OID_HEXSZ;
	const size_t header_len = strlen(header);

	char *buffer = *buffer_out;

	if (buffer + (header_len + sha_len + 1) > buffer_end)
		return GIT_EOBJCORRUPTED;

	if (memcmp(buffer, header, header_len) != 0)
		return GIT_EOBJCORRUPTED;

	if (buffer[header_len + sha_len] != '\n')
		return GIT_EOBJCORRUPTED;

	if (git_oid_mkstr(oid, buffer + header_len) < 0)
		return GIT_EOBJCORRUPTED;

	*buffer_out = buffer + (header_len + sha_len + 1);

	return 0;
}

int git__write_oid(git_odb_source *src, const char *header, const git_oid *oid)
{
	char hex_oid[41];

	git_oid_fmt(hex_oid, oid);
	hex_oid[40] = 0;

	return git__source_printf(src, "%s %s\n", header, hex_oid);
}

int git_commit__writeback(git_commit *commit, git_odb_source *src)
{
	git_commit_parents *parent;

	if (commit->tree == NULL)
		return GIT_ERROR;

	git__write_oid(src, "tree", git_tree_id(commit->tree));

	parent = commit->parents;

	while (parent != NULL) {
		git__write_oid(src, "parent", git_commit_id(parent->commit));
		parent = parent->next;
	}

	if (commit->author == NULL)
		return GIT_ERROR;

	git__write_person(src, "author", commit->author);

	if (commit->committer == NULL)
		return GIT_ERROR;

	git__write_person(src, "committer", commit->committer);

	if (commit->message != NULL)
		git__source_printf(src, "\n%s", commit->message);

	return GIT_SUCCESS;
}

int git_commit__parse_buffer(git_commit *commit, void *data, size_t len, unsigned int parse_flags)
{
	char *buffer = (char *)data;
	const char *buffer_end = (char *)data + len;

	git_oid oid;
	git_person person;

	if (git__parse_oid(&oid, &buffer, buffer_end, "tree ") < 0)
		return GIT_EOBJCORRUPTED;

	commit->tree = git_tree_lookup(commit->object.repo, &oid);

	/*
	 * TODO: commit grafts!
	 */

	clear_parents(commit);

	while (git__parse_oid(&oid, &buffer, buffer_end, "parent ") == 0) {
		git_commit *parent;
		git_commit_parents *node;

		if ((parent = git_commit_lookup(commit->object.repo, &oid)) == NULL)
			return GIT_ENOTFOUND;

		if ((node = git__malloc(sizeof(git_commit_parents))) == NULL)
			return GIT_ENOMEM;

		node->commit = parent;
		node->next = commit->parents;
		commit->parents = node;
	}

	if (git__parse_person(&person, &buffer, buffer_end, "author ") < 0)
		return GIT_EOBJCORRUPTED;

	if (parse_flags & COMMIT_FULL_PARSE) {
		if (commit->author)
			free(commit->author);

		commit->author = git__malloc(sizeof(git_person));
		memcpy(commit->author, &person, sizeof(git_person));
	}

	if (git__parse_person(&person, &buffer, buffer_end, "committer ") < 0)
		return GIT_EOBJCORRUPTED;

	commit->commit_time = person.time;

	if (parse_flags & COMMIT_FULL_PARSE) {
		if (commit->committer)
			free(commit->committer);

		commit->committer = git__malloc(sizeof(git_person));
		memcpy(commit->committer, &person, sizeof(git_person));
	}

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

	return 0;
}

#define GIT_COMMIT_GETTER(_rvalue, _name) \
	const _rvalue git_commit_##_name(git_commit *commit) \
	{\
		if (commit->_name) \
			return commit->_name; \
		git_commit__parse_full(commit); \
		return commit->_name; \
	}

GIT_COMMIT_GETTER(git_tree *, tree)
GIT_COMMIT_GETTER(git_person *, author)
GIT_COMMIT_GETTER(git_person *, committer)
GIT_COMMIT_GETTER(char *, message)
GIT_COMMIT_GETTER(char *, message_short)

time_t git_commit_time(git_commit *commit)
{
	if (commit->commit_time)
		return commit->commit_time;

	git_commit__parse_full(commit);
	return commit->commit_time;
}

void git_commit_set_tree(git_commit *commit, git_tree *tree)
{
	commit->object.modified = 1;
	commit->tree = tree;
}

void git_commit_set_author(git_commit *commit, const git_person *author)
{
	commit->object.modified = 1;
	if (commit->author == NULL)
		commit->author = git__malloc(sizeof(git_person));

	memcpy(commit->author, author, sizeof(git_person));
}

void git_commit_set_committer(git_commit *commit, const git_person *committer)
{
	commit->object.modified = 1;
	if (commit->committer == NULL)
		commit->committer = git__malloc(sizeof(git_person));

	memcpy(commit->committer, committer, sizeof(git_person));
}

void git_commit_set_message(git_commit *commit, const char *message)
{
	const char *short_message;

	commit->object.modified = 1;

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

	if ((node = git__malloc(sizeof(git_commit_parents))) == NULL)
		return;

	node->commit = new_parent;
	node->next = commit->parents;
	commit->parents = node;
}

