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
#include "hashtable.h"

#define COMMIT_SIZE(parent_count) (sizeof(commit) + (sizeof(commit_idx) * ((parent_count) - 1)))

typedef struct commit_object {
	git_oid oid;
	time_t time;
	unsigned int seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 parsed:1;

	unsigned short in_degree;
	unsigned short out_degree;

	struct commit_object **parents;
} commit_object;

typedef struct commit_node {
	commit_object *item;
	struct commit_node *next;
} commit_node;

struct git_revwalk {
	git_repository *repo;

	git_hashtable *commits;
	commit_node *iterator;
	git_vector pending;

	commit_node *(*insert)(commit_node **, commit_object *commit);

	unsigned walking:1;
	unsigned int sorting;
};

static commit_node *clist_insert(commit_node **list_p, commit_object *commit)
{
	commit_node *new_list = git__malloc(sizeof(commit_node));
	new_list->item = commit;
	new_list->next = *list_p;
	*list_p = new_list;
	return new_list;
}

static commit_node *clist_insert_date(commit_node **list, commit_object *commit)
{
	commit_node **pp = list;
	commit_node *p;

	while ((p = *pp) != NULL) {
		if (p->item->time < commit->time)
			break;
		pp = &p->next;
	}
	return clist_insert(pp, commit);
}

static commit_object *clist_pop(commit_node **stack)
{
	commit_node *top = *stack;
	commit_object *item = top ? top->item : NULL;

	if (top) {
		*stack = top->next;
		free(top);
	}

	return item;
}

static void clist_free(commit_node *list)
{
	while (list) {
		commit_node *temp = list;
		list = temp->next;
		free(temp);
	}
}

static void clist_datesort(commit_node **list)
{
	commit_node *ret = NULL;
	while (*list) {
		clist_insert_date(&ret, (*list)->item);
		*list = (*list)->next;
	}
	*list = ret;
}

static void clist_toposort(git_revwalk *walk, commit_node **list)
{
	commit_node *next, *orig = *list;
	commit_node *work, **insert;
	commit_node **pptr;

	unsigned short i;

	if (!orig)
		return;

	*list = NULL;

	/* update the indegree */
	for (next = orig; next != NULL; next = next->next) {
		for (i = 0; i < next->item->out_degree; ++i)
			next->item->parents[i]->in_degree++;
	}

	/*
	 * find the tips
	 *
	 * tips are nodes not reachable from any other node in the list
	 *
	 * the tips serve as a starting set for the work queue.
	 */
	work = NULL;
	insert = &work;
	for (next = orig; next != NULL; next = next->next) {
		commit_object *commit = next->item;

		if (commit->in_degree == 1)
			insert = &clist_insert(insert, commit)->next;
	}

	/* process the list in topological order */
	if (walk->sorting & GIT_SORT_TIME)
		clist_datesort(&work);

	pptr = list;
	*list = NULL;
	while (work) {
		commit_object *commit;
		commit_node *work_item;

		work_item = work;
		work = work_item->next;
		work_item->next = NULL;

		commit = work_item->item;
		for (i = 0; i < commit->out_degree; ++i) {
			commit_object *parent = commit->parents[i];

			if (!parent->in_degree)
				continue;

			/*
			 * parents are only enqueued for emission
			 * when all their children have been emitted thereby
			 * guaranteeing topological order.
			 */
			if (--parent->in_degree == 1)
				walk->insert(&work, parent);
		}
		/*
		 * work_item is a commit all of whose children
		 * have already been emitted. we can emit it now.
		 */
		commit->in_degree = 0;
		*pptr = work_item;
		pptr = &work_item->next;
	}
}



static uint32_t object_table_hash(const void *key, int hash_id)
{
	uint32_t r;
	git_oid *id;

	id = (git_oid *)key;
	memcpy(&r, id->id + (hash_id * sizeof(uint32_t)), sizeof(r));
	return r;
}

int git_revwalk_new(git_revwalk **revwalk_out, git_repository *repo)
{
	git_revwalk *walk;

	walk = git__malloc(sizeof(git_revwalk));
	if (walk == NULL)
		return GIT_ENOMEM;

	memset(walk, 0x0, sizeof(git_revwalk));

	walk->commits = git_hashtable_alloc(64,
			object_table_hash,
			(git_hash_keyeq_ptr)git_oid_cmp);

	if (walk->commits == NULL) {
		free(walk);
		return GIT_ENOMEM;
	}

	git_vector_init(&walk->pending, 8, NULL);

	walk->repo = repo;

	*revwalk_out = walk;
	return GIT_SUCCESS;
}

void git_revwalk_free(git_revwalk *walk)
{
	const void *_unused;
	commit_object *commit;

	if (walk == NULL)
		return;

	GIT_HASHTABLE_FOREACH(walk->commits, _unused, commit,
		free(commit);
	);

	git_revwalk_reset(walk);
	git_hashtable_free(walk->commits);
	git_vector_free(&walk->pending);
	free(walk);
}

git_repository *git_revwalk_repository(git_revwalk *walk)
{
	assert(walk);
	return walk->repo;
}

int git_revwalk_sorting(git_revwalk *walk, unsigned int sort_mode)
{
	assert(walk);

	if (walk->walking)
		return GIT_EBUSY;

	walk->sorting = sort_mode;
	git_revwalk_reset(walk);
	return GIT_SUCCESS;
}

static commit_object *commit_lookup(git_revwalk *walk, const git_oid *oid)
{
	commit_object *commit;

	if ((commit = git_hashtable_lookup(walk->commits, oid)) != NULL)
		return commit;

	commit = git__calloc(1, sizeof(commit_object));
	if (commit == NULL)
		return NULL;

	git_oid_cpy(&commit->oid, oid);

	if (git_hashtable_insert(walk->commits, &commit->oid, commit) < GIT_SUCCESS) {
		free(commit);
		return NULL;
	}

	return commit;
}

static int commit_quick_parse(git_revwalk *walk, commit_object *commit, git_rawobj *raw)
{
	const int parent_len = STRLEN("parent ") + GIT_OID_HEXSZ + 1;

	unsigned char *buffer = raw->data;
	unsigned char *buffer_end = buffer + raw->len;
	unsigned char *parents_start;

	int i, parents = 0;

	buffer += STRLEN("tree ") + GIT_OID_HEXSZ + 1;

	parents_start = buffer;
	while (buffer + parent_len < buffer_end && memcmp(buffer, "parent ", STRLEN("parent ")) == 0) {
		parents++;
		buffer += parent_len;
	}

	commit->parents = git__malloc(parents * sizeof(commit_object *));
	if (commit->parents == NULL)
		return GIT_ENOMEM;

	buffer = parents_start;
	for (i = 0; i < parents; ++i) {
		git_oid oid;

		if (git_oid_mkstr(&oid, (char *)buffer + STRLEN("parent ")) < GIT_SUCCESS)
			return GIT_EOBJCORRUPTED;

		commit->parents[i] = commit_lookup(walk, &oid);
		if (commit->parents[i] == NULL)
			return GIT_ENOMEM;

		buffer += parent_len;
	}

	commit->out_degree = parents;

	if ((buffer = memchr(buffer, '\n', buffer_end - buffer)) == NULL)
		return GIT_EOBJCORRUPTED;

	buffer = memchr(buffer, '>', buffer_end - buffer);
	if (buffer == NULL)
		return GIT_EOBJCORRUPTED;

	commit->time = strtol((char *)buffer + 2, NULL, 10);
	if (commit->time == 0)
		return GIT_EOBJCORRUPTED;

	commit->parsed = 1;
	return GIT_SUCCESS;
}

static int commit_parse(git_revwalk *walk, commit_object *commit)
{
	git_rawobj data;
	int error;

	if (commit->parsed)
		return GIT_SUCCESS;

	if ((error = git_odb_read(&data, walk->repo->db, &commit->oid)) < GIT_SUCCESS) {
		return error;
	}

	error = commit_quick_parse(walk, commit, &data);
	git_rawobj_close(&data);
	return error;
}

static void mark_uninteresting(commit_object *commit)
{
	unsigned short i;
	assert(commit);

	commit->uninteresting = 1;

	for (i = 0; i < commit->out_degree; ++i)
		if (!commit->parents[i]->uninteresting)
			mark_uninteresting(commit->parents[i]);
}

static int prepare_commit(git_revwalk *walk, commit_object *commit)
{
	int error;
	unsigned short i;

	if (commit->seen)
		return GIT_SUCCESS;

	commit->seen = 1;

	if ((error = commit_parse(walk, commit)) < GIT_SUCCESS)
			return error;

	if (commit->uninteresting)
		mark_uninteresting(commit);

	walk->insert(&walk->iterator, commit);

	for (i = 0; i < commit->out_degree && error == GIT_SUCCESS; ++i)
		error = prepare_commit(walk, commit->parents[i]);

	return error;
}

static int push_commit(git_revwalk *walk, const git_oid *oid, int uninteresting)
{
	commit_object *commit;

	commit = commit_lookup(walk, oid);
	if (commit == NULL)
		return GIT_ENOTFOUND;

	if (uninteresting)
		mark_uninteresting(commit);

	return git_vector_insert(&walk->pending, commit);
}

int git_revwalk_push(git_revwalk *walk, const git_oid *oid)
{
	assert(walk && oid);
	return push_commit(walk, oid, 0);
}

int git_revwalk_hide(git_revwalk *walk, const git_oid *oid)
{
	assert(walk && oid);
	return push_commit(walk, oid, 1);
}

static int prepare_walk(git_revwalk *walk)
{
	unsigned int i;
	int error;

	if (walk->sorting & GIT_SORT_TIME)
		walk->insert = &clist_insert_date;
	else
		walk->insert = &clist_insert;

	for (i = 0; i < walk->pending.length; ++i) {
		commit_object *commit = walk->pending.contents[i];
		if ((error = prepare_commit(walk, commit)) < GIT_SUCCESS) {
			return error;
		}
	}

	if (walk->sorting & GIT_SORT_TOPOLOGICAL)
		clist_toposort(walk, &walk->iterator);

	walk->walking = 1;
	return GIT_SUCCESS;
}

int git_revwalk_next(git_oid *oid, git_revwalk *walk)
{
	int error;
	commit_object *next;

	assert(walk && oid);

	if (!walk->walking) {
		if ((error = prepare_walk(walk)) < GIT_SUCCESS)
			return error;
	}

	while ((next = clist_pop(&walk->iterator)) != NULL) {
		if (!next->uninteresting) {
			git_oid_cpy(oid, &next->oid);
			return GIT_SUCCESS;
		}
	}

	return GIT_EREVWALKOVER;
}

void git_revwalk_reset(git_revwalk *walk)
{
	const void *_unused;
	commit_object *commit;

	assert(walk);

	GIT_HASHTABLE_FOREACH(walk->commits, _unused, commit,
		commit->seen = 0;
		commit->in_degree = 1;
	);

	if (walk->iterator) {
		clist_free(walk->iterator);
		walk->iterator = NULL;
	}

	walk->walking = 0;
}

