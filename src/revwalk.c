/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "odb.h"
#include "hashtable.h"
#include "pqueue.h"

#include "git2/revwalk.h"

typedef struct commit_object {
	git_oid oid;
	uint32_t time;
	unsigned int seen:1,
			 uninteresting:1,
			 topo_delay:1,
			 parsed:1;

	unsigned short in_degree;
	unsigned short out_degree;

	struct commit_object **parents;
} commit_object;

typedef struct commit_list {
	commit_object *item;
	struct commit_list *next;
} commit_list;

struct git_revwalk {
	git_repository *repo;

	git_hashtable *commits;

	commit_list *iterator_topo;
	commit_list *iterator_rand;
	commit_list *iterator_reverse;
	git_pqueue iterator_time;

	int (*get_next)(commit_object **, git_revwalk *);
	int (*enqueue)(git_revwalk *, commit_object *);

	git_vector memory_alloc;
	size_t chunk_size;

	unsigned walking:1;
	unsigned int sorting;
};

static commit_list *commit_list_insert(commit_object *item, commit_list **list_p)
{
	commit_list *new_list = git__malloc(sizeof(commit_list));
	new_list->item = item;
	new_list->next = *list_p;
	*list_p = new_list;
	return new_list;
}

static void commit_list_free(commit_list **list_p)
{
	commit_list *list = *list_p;

	while (list) {
		commit_list *temp = list;
		list = temp->next;
		git__free(temp);
	}

	*list_p = NULL;
}

static commit_object *commit_list_pop(commit_list **stack)
{
	commit_list *top = *stack;
	commit_object *item = top ? top->item : NULL;

	if (top) {
		*stack = top->next;
		git__free(top);
	}
	return item;
}

static int commit_time_cmp(void *a, void *b)
{
	commit_object *commit_a = (commit_object *)a;
	commit_object *commit_b = (commit_object *)b;

	return (commit_a->time < commit_b->time);
}

static uint32_t object_table_hash(const void *key, int hash_id)
{
	uint32_t r;
	const git_oid *id = key;

	memcpy(&r, id->id + (hash_id * sizeof(uint32_t)), sizeof(r));
	return r;
}

#define COMMITS_PER_CHUNK 128
#define CHUNK_STEP 64
#define PARENTS_PER_COMMIT ((CHUNK_STEP - sizeof(commit_object)) / sizeof(commit_object *))

static int alloc_chunk(git_revwalk *walk)
{
	void *chunk;

	chunk = git__calloc(COMMITS_PER_CHUNK, CHUNK_STEP);
	if (chunk == NULL)
		return GIT_ENOMEM;

	walk->chunk_size = 0;
	return git_vector_insert(&walk->memory_alloc, chunk);
}

static commit_object *alloc_commit(git_revwalk *walk)
{
	unsigned char *chunk;

	if (walk->chunk_size == COMMITS_PER_CHUNK)
		alloc_chunk(walk);

	chunk = git_vector_get(&walk->memory_alloc, walk->memory_alloc.length - 1);
	chunk += (walk->chunk_size * CHUNK_STEP);
	walk->chunk_size++;

	return (commit_object *)chunk;
}

static commit_object **alloc_parents(commit_object *commit, size_t n_parents)
{
	if (n_parents <= PARENTS_PER_COMMIT)
		return (commit_object **)((unsigned char *)commit + sizeof(commit_object));

	return git__malloc(n_parents * sizeof(commit_object *));
}


static commit_object *commit_lookup(git_revwalk *walk, const git_oid *oid)
{
	commit_object *commit;

	if ((commit = git_hashtable_lookup(walk->commits, oid)) != NULL)
		return commit;

	commit = alloc_commit(walk);
	if (commit == NULL)
		return NULL;

	git_oid_cpy(&commit->oid, oid);

	if (git_hashtable_insert(walk->commits, &commit->oid, commit) < GIT_SUCCESS) {
		git__free(commit);
		return NULL;
	}

	return commit;
}

static int commit_quick_parse(git_revwalk *walk, commit_object *commit, git_rawobj *raw)
{
	const int parent_len = strlen("parent ") + GIT_OID_HEXSZ + 1;

	unsigned char *buffer = raw->data;
	unsigned char *buffer_end = buffer + raw->len;
	unsigned char *parents_start;

	int i, parents = 0;
	int commit_time;

	buffer += strlen("tree ") + GIT_OID_HEXSZ + 1;

	parents_start = buffer;
	while (buffer + parent_len < buffer_end && memcmp(buffer, "parent ", strlen("parent ")) == 0) {
		parents++;
		buffer += parent_len;
	}

	commit->parents = alloc_parents(commit, parents);
	if (commit->parents == NULL)
		return GIT_ENOMEM;

	buffer = parents_start;
	for (i = 0; i < parents; ++i) {
		git_oid oid;

		if (git_oid_fromstr(&oid, (char *)buffer + strlen("parent ")) < GIT_SUCCESS)
			return git__throw(GIT_EOBJCORRUPTED, "Failed to parse commit. Parent object is corrupted");

		commit->parents[i] = commit_lookup(walk, &oid);
		if (commit->parents[i] == NULL)
			return GIT_ENOMEM;

		buffer += parent_len;
	}

	commit->out_degree = (unsigned short)parents;

	if ((buffer = memchr(buffer, '\n', buffer_end - buffer)) == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse commit. Object is corrupted");

	buffer = memchr(buffer, '>', buffer_end - buffer);
	if (buffer == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse commit. Can't find author");

	if (git__strtol32(&commit_time, (char *)buffer + 2, NULL, 10) < GIT_SUCCESS)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse commit. Can't parse commit time");

	commit->time = (time_t)commit_time;
	commit->parsed = 1;
	return GIT_SUCCESS;
}

static int commit_parse(git_revwalk *walk, commit_object *commit)
{
	git_odb_object *obj;
	int error;

	if (commit->parsed)
		return GIT_SUCCESS;

	if ((error = git_odb_read(&obj, walk->repo->db, &commit->oid)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to parse commit. Can't read object");

	if (obj->raw.type != GIT_OBJ_COMMIT) {
		git_odb_object_close(obj);
		return git__throw(GIT_EOBJTYPE, "Failed to parse commit. Object is no commit object");
	}

	error = commit_quick_parse(walk, commit, &obj->raw);
	git_odb_object_close(obj);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to parse commit");
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

static int process_commit(git_revwalk *walk, commit_object *commit, int hide)
{
	int error;

	if (hide)
		mark_uninteresting(commit);

	if (commit->seen)
		return GIT_SUCCESS;

	commit->seen = 1;

	if ((error = commit_parse(walk, commit)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to process commit");

	return walk->enqueue(walk, commit);
}

static int process_commit_parents(git_revwalk *walk, commit_object *commit)
{
	unsigned short i;
	int error = GIT_SUCCESS;

	for (i = 0; i < commit->out_degree && error == GIT_SUCCESS; ++i) {
		error = process_commit(walk, commit->parents[i], commit->uninteresting);
	}

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to process commit parents");
}

static int push_commit(git_revwalk *walk, const git_oid *oid, int uninteresting)
{
	commit_object *commit;

	commit = commit_lookup(walk, oid);
	if (commit == NULL)
		return git__throw(GIT_ENOTFOUND, "Failed to push commit. Object not found");

	return process_commit(walk, commit, uninteresting);
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

static int revwalk_enqueue_timesort(git_revwalk *walk, commit_object *commit)
{
	return git_pqueue_insert(&walk->iterator_time, commit);
}

static int revwalk_enqueue_unsorted(git_revwalk *walk, commit_object *commit)
{
	return commit_list_insert(commit, &walk->iterator_rand) ? GIT_SUCCESS : GIT_ENOMEM;
}

static int revwalk_next_timesort(commit_object **object_out, git_revwalk *walk)
{
	int error;
	commit_object *next;

	while ((next = git_pqueue_pop(&walk->iterator_time)) != NULL) {
		if ((error = process_commit_parents(walk, next)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to load next revision");

		if (!next->uninteresting) {
			*object_out = next;
			return GIT_SUCCESS;
		}
	}

	return git__throw(GIT_EREVWALKOVER, "Failed to load next revision");
}

static int revwalk_next_unsorted(commit_object **object_out, git_revwalk *walk)
{
	int error;
	commit_object *next;

	while ((next = commit_list_pop(&walk->iterator_rand)) != NULL) {
		if ((error = process_commit_parents(walk, next)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to load next revision");

		if (!next->uninteresting) {
			*object_out = next;
			return GIT_SUCCESS;
		}
	}

	return git__throw(GIT_EREVWALKOVER, "Failed to load next revision");
}

static int revwalk_next_toposort(commit_object **object_out, git_revwalk *walk)
{
	commit_object *next;
	unsigned short i;

	for (;;) {
		next = commit_list_pop(&walk->iterator_topo);
		if (next == NULL)
			return git__throw(GIT_EREVWALKOVER, "Failed to load next revision");

		if (next->in_degree > 0) {
			next->topo_delay = 1;
			continue;
		}

		for (i = 0; i < next->out_degree; ++i) {
			commit_object *parent = next->parents[i];

			if (--parent->in_degree == 0 && parent->topo_delay) {
				parent->topo_delay = 0;
				commit_list_insert(parent, &walk->iterator_topo);
			}
		}

		*object_out = next;
		return GIT_SUCCESS;
	}
}

static int revwalk_next_reverse(commit_object **object_out, git_revwalk *walk)
{
	*object_out = commit_list_pop(&walk->iterator_reverse);
	return *object_out ? GIT_SUCCESS : GIT_EREVWALKOVER;
}


static int prepare_walk(git_revwalk *walk)
{
	int error;
	commit_object *next;

	if (walk->sorting & GIT_SORT_TOPOLOGICAL) {
		unsigned short i;

		while ((error = walk->get_next(&next, walk)) == GIT_SUCCESS) {
			for (i = 0; i < next->out_degree; ++i) {
				commit_object *parent = next->parents[i];
				parent->in_degree++;
			}

			commit_list_insert(next, &walk->iterator_topo);
		}

		if (error != GIT_EREVWALKOVER)
			return git__rethrow(error, "Failed to prepare revision walk");

		walk->get_next = &revwalk_next_toposort;
	}

	if (walk->sorting & GIT_SORT_REVERSE) {

		while ((error = walk->get_next(&next, walk)) == GIT_SUCCESS)
			commit_list_insert(next, &walk->iterator_reverse);

		if (error != GIT_EREVWALKOVER)
			return git__rethrow(error, "Failed to prepare revision walk");

		walk->get_next = &revwalk_next_reverse;
	}

	walk->walking = 1;
	return GIT_SUCCESS;
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
		git__free(walk);
		return GIT_ENOMEM;
	}

	git_pqueue_init(&walk->iterator_time, 8, commit_time_cmp);
	git_vector_init(&walk->memory_alloc, 8, NULL);
	alloc_chunk(walk);

	walk->get_next = &revwalk_next_unsorted;
	walk->enqueue = &revwalk_enqueue_unsorted;

	walk->repo = repo;

	*revwalk_out = walk;
	return GIT_SUCCESS;
}

void git_revwalk_free(git_revwalk *walk)
{
	unsigned int i;
	const void *GIT_UNUSED(_unused);
	commit_object *commit;

	if (walk == NULL)
		return;

	git_revwalk_reset(walk);

	/* if the parent has more than PARENTS_PER_COMMIT parents,
	 * we had to allocate a separate array for those parents.
	 * make sure it's being free'd */
	GIT_HASHTABLE_FOREACH(walk->commits, _unused, commit, {
		if (commit->out_degree > PARENTS_PER_COMMIT)
			git__free(commit->parents);
	});

	git_hashtable_free(walk->commits);
	git_pqueue_free(&walk->iterator_time);

	for (i = 0; i < walk->memory_alloc.length; ++i)
		git__free(git_vector_get(&walk->memory_alloc, i));

	git_vector_free(&walk->memory_alloc);
	git__free(walk);
}

git_repository *git_revwalk_repository(git_revwalk *walk)
{
	assert(walk);
	return walk->repo;
}

void git_revwalk_sorting(git_revwalk *walk, unsigned int sort_mode)
{
	assert(walk);

	if (walk->walking)
		git_revwalk_reset(walk);

	walk->sorting = sort_mode;

	if (walk->sorting & GIT_SORT_TIME) {
		walk->get_next = &revwalk_next_timesort;
		walk->enqueue = &revwalk_enqueue_timesort;
	} else {
		walk->get_next = &revwalk_next_unsorted;
		walk->enqueue = &revwalk_enqueue_unsorted;
	}
}

int git_revwalk_next(git_oid *oid, git_revwalk *walk)
{
	int error;
	commit_object *next;

	assert(walk && oid);

	if (!walk->walking) {
		if ((error = prepare_walk(walk)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to load next revision");
	}

	error = walk->get_next(&next, walk);

	if (error == GIT_EREVWALKOVER) {
		git_revwalk_reset(walk);
		return GIT_EREVWALKOVER;
	}

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to load next revision");

	git_oid_cpy(oid, &next->oid);
	return GIT_SUCCESS;
}

void git_revwalk_reset(git_revwalk *walk)
{
	const void *GIT_UNUSED(_unused);
	commit_object *commit;

	assert(walk);

	GIT_HASHTABLE_FOREACH(walk->commits, _unused, commit,
		commit->seen = 0;
		commit->in_degree = 0;
		commit->topo_delay = 0;
	);

	git_pqueue_clear(&walk->iterator_time);
	commit_list_free(&walk->iterator_topo);
	commit_list_free(&walk->iterator_rand);
	commit_list_free(&walk->iterator_reverse);
	walk->walking = 0;
}

