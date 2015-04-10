/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdint.h>
#include "alloc.h"
#include "allocmap.h"
#include "bench_util.h"

GIT__USE_ALLOCMAP

static git_allocmap *alloc_map = NULL;
static uint32_t alloc_run = 0;

static bool alloc_profile = false;

static struct gitbench_alloc_stat_t alloc_stats = {0};

int gitbench_alloc_init(void)
{
	int error;

	assert(alloc_map == NULL);

	error = git_allocmap_alloc(&alloc_map);
	alloc_profile = true;

	return error;
}

void gitbench_alloc_start(void)
{
	assert(alloc_map);
	alloc_run++;

	alloc_stats.run_alloc_count = 0;
	alloc_stats.run_dealloc_count = 0;
	alloc_stats.run_alloc_current = 0;
	alloc_stats.run_alloc_max = 0;
}

void gitbench_alloc_stop(void)
{
	assert(alloc_map);
}

struct gitbench_alloc_stat_t *gitbench_alloc_stats(void)
{
	return &alloc_stats;
}

void gitbench_alloc_shutdown(void)
{
	assert(alloc_map);
	git_allocmap_free(alloc_map);
	alloc_map = NULL;
}


GIT_INLINE(void) update_stats(size_t new_len, size_t old_len)
{
	if (new_len > 0) {
		alloc_stats.total_alloc_count++;
		alloc_stats.run_alloc_count++;
	}

	if (old_len > 0) {
		alloc_stats.total_dealloc_count++;
		alloc_stats.run_dealloc_count++;
	}

	if (new_len > old_len) {
		size_t diff = new_len - old_len;

		alloc_stats.total_alloc_max = max(alloc_stats.total_alloc_max, diff);
		alloc_stats.run_alloc_max = max(alloc_stats.run_alloc_max, diff);
		
		alloc_stats.total_alloc_current += diff;
		alloc_stats.run_alloc_current += diff;
	} else {
		size_t diff = old_len - new_len;

		alloc_stats.total_alloc_current -= diff;
		alloc_stats.run_alloc_current -= diff;
	}
}

GIT_INLINE(void *) gitbench__malloc_standard(size_t len)
{
	void *p = malloc(len);
	if (p == NULL)
		giterr_set_oom();
	return p;
}

GIT_INLINE(void *) gitbench__malloc_profile(size_t len)
{
	void *p = gitbench__malloc_standard(len);
	int error;

	if (p) {
		git_allocmap_insert(alloc_map, p, len, error);
		assert(error >= 0);

		if (verbosity > 1)
			printf("::::: Allocated %p (%" PRIuZ ")\n", p, len);

		update_stats(len, 0);
	}

	return p;
}

void *gitbench__calloc(size_t nelem, size_t elsize)
{
	size_t len;
	void *p;

	/* TODO: mult w/ overflow */
	len = nelem * elsize;

	if ((p = gitbench__malloc(len)) == NULL)
		return NULL;

	memset(p, 0, len);
	return p;
}

GIT_INLINE(void *) gitbench__realloc_standard(void *ptr, size_t len)
{
	void *p;

	if ((p = realloc(ptr, len)) == NULL)
		giterr_set_oom();

	return p;
}

GIT_INLINE(void *) gitbench__realloc_profile(void *old_ptr, size_t new_len)
{
	khiter_t old_idx;
	size_t old_len;
	void *new_ptr;
	int error;

	if (!old_ptr)
		return gitbench__malloc_profile(new_len);

	if ((new_ptr = gitbench__realloc_standard(old_ptr, new_len)) == NULL)
		return NULL;

	old_idx = git_allocmap_lookup_index(alloc_map, old_ptr);
	assert(git_allocmap_valid_index(alloc_map, old_idx));

	if (old_ptr == new_ptr) {
		old_len = git_allocmap_value_at(alloc_map, old_idx);
		git_allocmap_set_value_at(alloc_map, old_idx, new_len);
	} else {
		old_len = git_allocmap_value_at(alloc_map, old_idx);
		git_allocmap_delete_at(alloc_map, old_idx);

		git_allocmap_insert(alloc_map, new_ptr, new_len, error);
		assert(error >= 0);
	}

	update_stats(new_len, old_len);
	return new_ptr;
}

GIT_INLINE(void) gitbench__free_standard(void *ptr)
{
	free(ptr);
}

GIT_INLINE(void) gitbench__free_profile(void *ptr)
{
	khiter_t idx;
	size_t len;

	if (!ptr)
		return;

	free(ptr);

	idx = git_allocmap_lookup_index(alloc_map, ptr);
	if (!git_allocmap_valid_index(alloc_map, idx))
		return;

	len = git_allocmap_value_at(alloc_map, idx);
	git_allocmap_delete_at(alloc_map, idx);

	if (verbosity > 1)
		printf("::::: Dellocated %p (%" PRIuZ ")\n", ptr, len);

	update_stats(0, len);
}

char *gitbench__strdup(const char *str)
{
	char *dup;
	size_t len = strlen(str);

	if ((dup = gitbench__malloc(len + 1)) == NULL)
		return NULL;

	memcpy(dup, str, len + 1);
	return dup;
}


void *gitbench__malloc(size_t len)
{
	return alloc_profile ? gitbench__malloc_profile(len) :
		gitbench__malloc_standard(len);
}

void *gitbench__realloc(void *ptr, size_t len)
{
	return alloc_profile ? gitbench__realloc_profile(ptr, len) :
		gitbench__realloc_standard(ptr, len);
}

void gitbench__free(void *ptr)
{
	alloc_profile ? gitbench__free_profile(ptr) :
		gitbench__free_standard(ptr);
}
