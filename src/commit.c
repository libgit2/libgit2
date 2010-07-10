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

#define COMMIT_PRINT(commit) {\
	char oid[41]; oid[40] = 0;\
	git_oid_fmt(oid, &commit->object.id);\
	printf("Oid: %s | In degree: %d | Time: %u\n", oid, commit->in_degree, commit->commit_time);\
}

void git_commit__free(git_commit *commit)
{
	git_commit_list_clear(&commit->parents, 0);

	if (commit->odb_open)
		git_obj_close(&commit->odb_object);

	free(commit->author);
	free(commit->committer);
	free(commit->message);
	free(commit->message_short);
	free(commit);
}

const git_oid *git_commit_id(git_commit *c)
{
	return &c->object.id;
}

void git_commit__mark_uninteresting(git_commit *commit)
{
	git_commit_node *parents;

	if (commit == NULL)
		return;

	parents = commit->parents.head;

	commit->uninteresting = 1;

	while (parents) {
		parents->commit->uninteresting = 1;
		parents = parents->next;
	}
}

git_commit *git_commit_parse(git_revpool *pool, const git_oid *id)
{
	git_commit *commit = NULL;

	if ((commit = git_commit_lookup(pool, id)) == NULL)
		return NULL;

	if (git_commit__parse_basic(commit) < 0)
		goto error_cleanup;

	return commit;

error_cleanup:
	/* FIXME: do not free; the commit is owned by the revpool */
	free(commit);
	return NULL;
}

int git_commit__parse(git_commit *commit, unsigned int parse_flags, int close_db_object)
{
	int error = 0;

	if (!commit->odb_open) {
		error = git_odb_read(&commit->odb_object, commit->object.pool->db, &commit->object.id);
		if (error < 0)
			return error;

		if (commit->odb_object.type != GIT_OBJ_COMMIT) {
			git_obj_close(&commit->odb_object);
			return GIT_EOBJTYPE;
		}

		commit->odb_open = 1;
	}

	error = git_commit__parse_buffer(commit,
			commit->odb_object.data, commit->odb_object.len, parse_flags);

	if (close_db_object) {
		git_obj_close(&commit->odb_object);
		commit->odb_open = 0;
	}

	return error;
}

int git_commit__parse_basic(git_commit *commit)
{
	int error;

	if (commit->basic_parse)
		return 0;

	error = git_commit__parse(commit,
			(GIT_COMMIT_TREE | GIT_COMMIT_PARENTS | GIT_COMMIT_TIME), 1);

	if (error < 0)
		return error;

	commit->basic_parse = 1;
	return 0;
}

git_commit *git_commit_lookup(git_revpool *pool, const git_oid *id)
{
	git_commit *commit = NULL;

	if (pool == NULL)
		return NULL;

	commit = (git_commit *)git_revpool_table_lookup(pool->objects, id);
	if (commit != NULL)
		return commit;

	commit = git__malloc(sizeof(git_commit));

	if (commit == NULL)
		return NULL;

	memset(commit, 0x0, sizeof(git_commit));

	/* Initialize parent object */
	git_oid_cpy(&commit->object.id, id);
	commit->object.pool = pool;
	commit->object.type = GIT_OBJ_COMMIT;

	git_revpool_table_insert(pool->objects, (git_revpool_object *)commit);

	return commit;
}

int git_commit__parse_person(git_commit_person *person, char **buffer_out,
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

int git_commit__parse_oid(git_oid *oid, char **buffer_out,
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

int git_commit__parse_buffer(git_commit *commit, void *data, size_t len, unsigned int parse_flags)
{
	char *buffer = (char *)data;
	const char *buffer_end = (char *)data + len;

	git_oid oid;
	git_commit_person person;

	if (git_commit__parse_oid(&oid, &buffer, buffer_end, "tree ") < 0)
		return GIT_EOBJCORRUPTED;

	if (parse_flags & GIT_COMMIT_TREE)
		commit->tree = git_tree_lookup(commit->object.pool, &oid);

	/*
	 * TODO: commit grafts!
	 */

	if (parse_flags & GIT_COMMIT_PARENTS)
		git_commit_list_clear(&commit->parents, 0);

	while (git_commit__parse_oid(&oid, &buffer, buffer_end, "parent ") == 0) {
		git_commit *parent;

		if ((parse_flags & GIT_COMMIT_PARENTS) == 0)
			continue;

		if ((parent = git_commit_lookup(commit->object.pool, &oid)) == NULL)
			return GIT_ENOTFOUND;

		/* Inherit uninteresting flag */
		if (commit->uninteresting)
			parent->uninteresting = 1;

		if (git_commit_list_push_back(&commit->parents, parent) < 0)
			return GIT_ENOMEM;
	}

	if (git_commit__parse_person(&person, &buffer, buffer_end, "author ") < 0)
		return GIT_EOBJCORRUPTED;

	if (parse_flags & GIT_COMMIT_AUTHOR) {
		if (commit->author)
			free(commit->author);

		commit->author = git__malloc(sizeof(git_commit_person));
		memcpy(commit->author, &person, sizeof(git_commit_person));
	}

	if (git_commit__parse_person(&person, &buffer, buffer_end, "committer ") < 0)
		return GIT_EOBJCORRUPTED;

	if (parse_flags & GIT_COMMIT_TIME)
		commit->commit_time = person.time;

	if (parse_flags & GIT_COMMIT_COMMITTER) {
		if (commit->committer)
			free(commit->committer);

		commit->committer = git__malloc(sizeof(git_commit_person));
		memcpy(commit->committer, &person, sizeof(git_commit_person));
	}

	/* parse commit message */
	while (buffer <= buffer_end && *buffer == '\n')
		buffer++;

	if (buffer < buffer_end)
	{
		if (parse_flags & GIT_COMMIT_MESSAGE) {
			size_t message_len = buffer_end - buffer;

			commit->message = git__malloc(message_len + 1);
			memcpy(commit->message, buffer, message_len);
			commit->message[message_len] = 0;
		}

		if (parse_flags & GIT_COMMIT_MESSAGE_SHORT) {
			char *line_end;
			size_t message_len;

			line_end = memchr(buffer, '\n', buffer_end - buffer);
			message_len = line_end - buffer;

			commit->message_short = git__malloc(message_len + 1);
			memcpy(commit->message_short, buffer, message_len);
			commit->message_short[message_len] = 0;
		}
	}

	return 0;
}

const git_tree *git_commit_tree(git_commit *commit)
{
	if (commit->tree)
		return commit->tree;

	git_commit__parse(commit, GIT_COMMIT_TREE, 0);
	return commit->tree;
}

const git_commit_person *git_commit_author(git_commit *commit)
{
	if (commit->author)
		return commit->author;

	git_commit__parse(commit, GIT_COMMIT_AUTHOR, 0);
	return commit->author;
}

const git_commit_person *git_commit_committer(git_commit *commit)
{
	if (commit->committer)
		return commit->committer;

	git_commit__parse(commit, GIT_COMMIT_COMMITTER, 0);
	return commit->committer;
}

time_t git_commit_time(git_commit *commit)
{
	if (commit->commit_time)
		return commit->commit_time;

	git_commit__parse(commit, GIT_COMMIT_TIME, 0);
	return commit->commit_time;
}

const char *git_commit_message(git_commit *commit)
{
	if (commit->message)
		return commit->message;

	git_commit__parse(commit, GIT_COMMIT_MESSAGE, 0);
	return commit->message;
}

const char *git_commit_message_short(git_commit *commit)
{
	if (commit->message_short)
		return commit->message_short;

	git_commit__parse(commit, GIT_COMMIT_MESSAGE_SHORT, 0);
	return commit->message_short;
}



int git_commit_list_push_back(git_commit_list *list, git_commit *commit)
{
	git_commit_node *node = NULL;

	node = git__malloc(sizeof(git_commit_list));

	if (node == NULL)
		return GIT_ENOMEM;

	node->commit = commit;
	node->next = NULL;
	node->prev = list->tail;

	if (list->tail == NULL) {
		list->head = list->tail = node;
	} else {
		list->tail->next = node;
		list->tail = node;
	}

	list->size++;
	return 0;
}

int git_commit_list_push_front(git_commit_list *list, git_commit *commit)
{
	git_commit_node *node = NULL;

	node = git__malloc(sizeof(git_commit_list));

	if (node == NULL)
		return GIT_ENOMEM;

	node->commit = commit;
	node->next = list->head;
	node->prev = NULL;

	if (list->head == NULL) {
		list->head = list->tail = node;
	} else {
		list->head->prev = node;
		list->head = node;
	}

	list->size++;
	return 0;
}


git_commit *git_commit_list_pop_back(git_commit_list *list)
{
	git_commit_node *node;
	git_commit *commit;

	if (list->tail == NULL)
		return NULL;

	node = list->tail;
	list->tail = list->tail->prev;
	if (list->tail == NULL)
		list->head = NULL;

	commit = node->commit;
	free(node);

	list->size--;

	return commit;
}

git_commit *git_commit_list_pop_front(git_commit_list *list)
{
	git_commit_node *node;
	git_commit *commit;

	if (list->head == NULL)
		return NULL;

	node = list->head;
	list->head = list->head->next;
	if (list->head == NULL)
		list->tail = NULL;

	commit = node->commit;
	free(node);

	list->size--;

	return commit;
}

void git_commit_list_clear(git_commit_list *list, int free_commits)
{
	git_commit_node *node, *next_node;

	node = list->head;
	while (node) {
		if (free_commits)
			free(node->commit);

		next_node = node->next;
		free(node);
		node = next_node;
	}

	list->head = list->tail = NULL;
	list->size = 0;
}

void git_commit_list_timesort(git_commit_list *list)
{
	git_commit_node *p, *q, *e;
	int in_size, p_size, q_size, merge_count, i;

	if (list->head == NULL)
		return;

	in_size = 1;

	do {
		p = list->head;
		list->tail = NULL;
		merge_count = 0;

		while (p != NULL) {
			merge_count++;
			q = p;
			p_size = 0;
			q_size = in_size;

			for (i = 0; i < in_size && q; ++i, q = q->next)
				p_size++;

			while (p_size > 0 || (q_size > 0 && q)) {

				if (p_size == 0)
					e = q, q = q->next, q_size--;

				else if (q_size == 0 || q == NULL ||
						p->commit->commit_time >= q->commit->commit_time)
					e = p, p = p->next, p_size--;

				else
					e = q, q = q->next, q_size--;

				if (list->tail != NULL)
					list->tail->next = e;
				else
					list->head = e;

				e->prev = list->tail;
				list->tail = e;
			}

			p = q;
		}

		list->tail->next = NULL;
		in_size *= 2;

	} while (merge_count > 1);
}

void git_commit_list_toposort(git_commit_list *list)
{
	git_commit *commit;
	git_commit_list topo;
	memset(&topo, 0x0, sizeof(git_commit_list));

	while ((commit = git_commit_list_pop_back(list)) != NULL) {
		git_commit_node *p;

		if (commit->in_degree > 0) {
			commit->topo_delay = 1;
			continue;
		}

		for (p = commit->parents.head; p != NULL; p = p->next) {
			p->commit->in_degree--;

			if (p->commit->in_degree == 0 && p->commit->topo_delay) {
				p->commit->topo_delay = 0;
				git_commit_list_push_back(list, p->commit);
			}
		}

		git_commit_list_push_back(&topo, commit);
	}

	list->head = topo.head;
	list->tail = topo.tail;
	list->size = topo.size;
}

