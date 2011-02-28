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

uint32_t git_revwalk__commit_hash(const void *key, int hash_id)
{
	uint32_t r;
	git_commit *commit;

	commit = (git_commit *)key;
	memcpy(&r, commit->object.id.id + (hash_id * sizeof(uint32_t)), sizeof(r));
	return r;
}

int git_revwalk__commit_keycmp(const void *key_a, const void *key_b)
{
	git_commit *a = (git_commit *)key_a;
	git_commit *b = (git_commit *)key_b;
	return git_oid_cmp(&a->object.id, &b->object.id);
}

int git_revwalk_new(git_revwalk **revwalk_out, git_repository *repo)
{
	git_revwalk *walk;

	walk = git__malloc(sizeof(git_revwalk));
	if (walk == NULL)
		return GIT_ENOMEM;

	memset(walk, 0x0, sizeof(git_revwalk));

	walk->commits = git_hashtable_alloc(64,
			git_revwalk__commit_hash,
			git_revwalk__commit_keycmp);

	if (walk->commits == NULL) {
		free(walk);
		return GIT_ENOMEM;
	}

	walk->repo = repo;

	*revwalk_out = walk;
	return GIT_SUCCESS;
}

void git_revwalk_free(git_revwalk *walk)
{
	if (walk == NULL)
		return;

	git_revwalk_reset(walk);
	git_hashtable_free(walk->commits);
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

static git_revwalk_commit *commit_to_walkcommit(git_revwalk *walk, git_commit *commit_object)
{
	git_revwalk_commit *commit;

	commit = (git_revwalk_commit *)git_hashtable_lookup(walk->commits, commit_object);

	if (commit != NULL)
		return commit;

	commit = git__malloc(sizeof(git_revwalk_commit));
	if (commit == NULL)
		return NULL;

	memset(commit, 0x0, sizeof(git_revwalk_commit));

	commit->commit_object = commit_object;
	GIT_OBJECT_INCREF(commit_object);

	git_hashtable_insert(walk->commits, commit_object, commit);

	return commit;
}

static git_revwalk_commit *insert_commit(git_revwalk *walk, git_commit *commit_object)
{
	git_revwalk_commit *commit;
	unsigned int i;

	assert(walk && commit_object);

	if (commit_object->object.repo != walk->repo || walk->walking)
		return NULL;

	commit = commit_to_walkcommit(walk, commit_object);
	if (commit == NULL)
		return NULL;

	if (commit->seen)
		return commit;

	commit->seen = 1;

	for (i = 0; i < commit->commit_object->parents.length; ++i) {
		git_commit *parent_object;
		git_revwalk_commit *parent;

		parent_object = git_vector_get(&commit->commit_object->parents, i);

		if ((parent = commit_to_walkcommit(walk, parent_object)) == NULL)
			return NULL;

		parent = insert_commit(walk, parent_object);
		if (parent == NULL)
			return NULL;

		parent->in_degree++;

		git_revwalk_list_push_back(&commit->parents, parent);
	}

	if (git_revwalk_list_push_back(&walk->iterator, commit))
		return NULL;

	return commit;
}

int git_revwalk_push(git_revwalk *walk, git_commit *commit)
{
	assert(walk && commit);
	return insert_commit(walk, commit) ? GIT_SUCCESS : GIT_ENOMEM;
}

static void mark_uninteresting(git_revwalk_commit *commit)
{
	git_revwalk_listnode *parent;

	assert(commit);

	commit->uninteresting = 1;
	parent = commit->parents.head;

	while (parent) {
		mark_uninteresting(parent->walk_commit);
		parent = parent->next;
	}
}

int git_revwalk_hide(git_revwalk *walk, git_commit *commit)
{
	git_revwalk_commit *hide;

	assert(walk && commit);
	
	hide = insert_commit(walk, commit);
	if (hide == NULL)
		return GIT_ENOMEM;

	mark_uninteresting(hide);
	return GIT_SUCCESS;
}


static void prepare_walk(git_revwalk *walk)
{
	if (walk->sorting & GIT_SORT_TIME)
		git_revwalk_list_timesort(&walk->iterator);

	if (walk->sorting & GIT_SORT_TOPOLOGICAL)
		git_revwalk_list_toposort(&walk->iterator);

	if (walk->sorting & GIT_SORT_REVERSE)
		walk->next = &git_revwalk_list_pop_back;
	else
		walk->next = &git_revwalk_list_pop_front;

	walk->walking = 1;
}

int git_revwalk_next(git_commit **commit, git_revwalk *walk)
{
	git_revwalk_commit *next;

	assert(walk && commit);

	if (!walk->walking)
		prepare_walk(walk);

	*commit = NULL;

	while ((next = walk->next(&walk->iterator)) != NULL) {
		if (!next->uninteresting) {
			*commit = next->commit_object;
			GIT_OBJECT_INCREF(*commit);
			return GIT_SUCCESS;
		}
	}

	/* No commits left to iterate */
	git_revwalk_reset(walk);
	return GIT_EREVWALKOVER;
}

void git_revwalk_reset(git_revwalk *walk)
{
	const void *_unused;
	git_revwalk_commit *commit;

	assert(walk);

	GIT_HASHTABLE_FOREACH(walk->commits, _unused, commit, {
		git_object_close((git_object *)commit->commit_object);
		git_revwalk_list_clear(&commit->parents);
		free(commit);
	});

	git_hashtable_clear(walk->commits);
	git_revwalk_list_clear(&walk->iterator);
	walk->walking = 0;
}






int git_revwalk_list_push_back(git_revwalk_list *list, git_revwalk_commit *commit)
{
	git_revwalk_listnode *node = NULL;

	node = git__malloc(sizeof(git_revwalk_listnode));

	if (node == NULL)
		return GIT_ENOMEM;

	node->walk_commit = commit;
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

int git_revwalk_list_push_front(git_revwalk_list *list, git_revwalk_commit *commit)
{
	git_revwalk_listnode *node = NULL;

	node = git__malloc(sizeof(git_revwalk_listnode));

	if (node == NULL)
		return GIT_ENOMEM;

	node->walk_commit = commit;
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


git_revwalk_commit *git_revwalk_list_pop_back(git_revwalk_list *list)
{
	git_revwalk_listnode *node;
	git_revwalk_commit *commit;

	if (list->tail == NULL)
		return NULL;

	node = list->tail;
	list->tail = list->tail->prev;
	if (list->tail == NULL)
		list->head = NULL;
	else
		list->tail->next = NULL;

	commit = node->walk_commit;
	free(node);

	list->size--;

	return commit;
}

git_revwalk_commit *git_revwalk_list_pop_front(git_revwalk_list *list)
{
	git_revwalk_listnode *node;
	git_revwalk_commit *commit;

	if (list->head == NULL)
		return NULL;

	node = list->head;
	list->head = list->head->next;
	if (list->head == NULL)
		list->tail = NULL;
	else
		list->head->prev = NULL;

	commit = node->walk_commit;
	free(node);

	list->size--;

	return commit;
}

void git_revwalk_list_clear(git_revwalk_list *list)
{
	git_revwalk_listnode *node, *next_node;

	node = list->head;
	while (node) {
		next_node = node->next;
		free(node);
		node = next_node;
	}

	list->head = list->tail = NULL;
	list->size = 0;
}

void git_revwalk_list_timesort(git_revwalk_list *list)
{
	git_revwalk_listnode *p, *q, *e;
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
						p->walk_commit->commit_object->committer->when.time >= 
						q->walk_commit->commit_object->committer->when.time)
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

void git_revwalk_list_toposort(git_revwalk_list *list)
{
	git_revwalk_commit *commit;
	git_revwalk_list topo;
	memset(&topo, 0x0, sizeof(git_revwalk_list));

	while ((commit = git_revwalk_list_pop_back(list)) != NULL) {
		git_revwalk_listnode *p;

		if (commit->in_degree > 0) {
			commit->topo_delay = 1;
			continue;
		}

		for (p = commit->parents.head; p != NULL; p = p->next) {
			p->walk_commit->in_degree--;

			if (p->walk_commit->in_degree == 0 && p->walk_commit->topo_delay) {
				p->walk_commit->topo_delay = 0;
				git_revwalk_list_push_back(list, p->walk_commit);
			}
		}

		git_revwalk_list_push_back(&topo, commit);
	}

	list->head = topo.head;
	list->tail = topo.tail;
	list->size = topo.size;
}

