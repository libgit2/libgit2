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
#include "pqueue.h"

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

struct git_revwalk {
	git_repository *repo;

	git_hashtable *commits;
	git_pqueue iterator;
	git_vector pending;

	git_vector memory_alloc;
	size_t chunk_size;

	unsigned walking:1;
	unsigned int sorting;
};

static int commit_time_cmp(void *a, void *b)
{
	commit_object *commit_a = (commit_object *)a;
	commit_object *commit_b = (commit_object *)b;

	return (commit_a->time < commit_b->time);
}

static uint32_t object_table_hash(const void *key, int hash_id)
{
	uint32_t r;
	git_oid *id;

	id = (git_oid *)key;
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
	git_vector_init(&walk->memory_alloc, 8, NULL);
	alloc_chunk(walk);

	walk->repo = repo;

	*revwalk_out = walk;
	return GIT_SUCCESS;
}

void git_revwalk_free(git_revwalk *walk)
{
	unsigned int i;

	if (walk == NULL)
		return;

	git_revwalk_reset(walk);
	git_hashtable_free(walk->commits);
	git_vector_free(&walk->pending);

	for (i = 0; i < walk->memory_alloc.length; ++i) {
		free(git_vector_get(&walk->memory_alloc, i));
	}

	git_vector_free(&walk->memory_alloc);
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

	commit = alloc_commit(walk);
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

	commit->parents = alloc_parents(commit, parents);
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

	commit->out_degree = (unsigned short)parents;

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

static int process_commit(git_revwalk *walk, commit_object *commit)
{
	int error;

	if (commit->seen)
		return GIT_SUCCESS;

	commit->seen = 1;

	if ((error = commit_parse(walk, commit)) < GIT_SUCCESS)
			return error;

	if (commit->uninteresting)
		mark_uninteresting(commit);

	return git_pqueue_insert(&walk->iterator, commit);
}

static int process_commit_parents(git_revwalk *walk, commit_object *commit)
{
	unsigned short i;
	int error = GIT_SUCCESS;

	for (i = 0; i < commit->out_degree && error == GIT_SUCCESS; ++i) {
		error = process_commit(walk, commit->parents[i]);
	}

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

	if ((error = git_pqueue_init(&walk->iterator, 32, commit_time_cmp)) < GIT_SUCCESS)
		return error;

	for (i = 0; i < walk->pending.length; ++i) {
		commit_object *commit = walk->pending.contents[i];
		if ((error = process_commit(walk, commit)) < GIT_SUCCESS) {
			return error;
		}
	}

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

	while ((next = git_pqueue_pop(&walk->iterator)) != NULL) {
		if ((error = process_commit_parents(walk, next)) < GIT_SUCCESS)
			return error;

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

	git_pqueue_free(&walk->iterator);
	walk->walking = 0;
}

