/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "sizemap.h"

#define kmalloc git__malloc
#define kcalloc git__calloc
#define krealloc git__realloc
#define kreallocarray git__reallocarray
#define kfree git__free
#include "khash.h"

__KHASH_TYPE(size, git_object_size_t, void *)

__KHASH_IMPL(size, static kh_inline, git_object_size_t, void *, 1, kh_int64_hash_func, kh_int64_hash_equal)


int git_sizemap_new(git_sizemap **out)
{
	*out = kh_init(size);
	GIT_ERROR_CHECK_ALLOC(*out);

	return 0;
}

void git_sizemap_free(git_sizemap *map)
{
	kh_destroy(size, map);
}

void git_sizemap_clear(git_sizemap *map)
{
	kh_clear(size, map);
}

size_t git_sizemap_size(git_sizemap *map)
{
	return kh_size(map);
}

void *git_sizemap_get(git_sizemap *map, const git_object_size_t key)
{
	size_t idx = kh_get(size, map, key);
	if (idx == kh_end(map) || !kh_exist(map, idx))
		return NULL;
	return kh_val(map, idx);
}

int git_sizemap_set(git_sizemap *map, const git_object_size_t key, void *value)
{
	size_t idx;
	int rval;

	idx = kh_put(size, map, key, &rval);
	if (rval < 0)
		return -1;

	if (rval == 0)
		kh_key(map, idx) = key;

	kh_val(map, idx) = value;

	return 0;
}

int git_sizemap_delete(git_sizemap *map, const git_object_size_t key)
{
	khiter_t idx = kh_get(size, map, key);
	if (idx == kh_end(map))
		return GIT_ENOTFOUND;
	kh_del(size, map, idx);
	return 0;
}

int git_sizemap_exists(git_sizemap *map, const git_object_size_t key)
{
	return kh_get(size, map, key) != kh_end(map);
}

int git_sizemap_iterate(void **value, git_sizemap *map, size_t *iter, git_object_size_t *key)
{
	size_t i = *iter;

	while (i < map->n_buckets && !kh_exist(map, i))
		i++;

	if (i >= map->n_buckets)
		return GIT_ITEROVER;

	if (key)
		*key = kh_key(map, i);
	if (value)
		*value = kh_value(map, i);
	*iter = ++i;

	return 0;
}
