/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "offmap.h"

#define kmalloc git__malloc
#define kcalloc git__calloc
#define krealloc git__realloc
#define kreallocarray git__reallocarray
#define kfree git__free
#include "khash.h"

__KHASH_TYPE(off, git_off_t, void *)

__KHASH_IMPL(off, static kh_inline, git_off_t, void *, 1, kh_int64_hash_func, kh_int64_hash_equal)


int git_offmap_new(git_offmap **out)
{
	*out = kh_init(off);
	GIT_ERROR_CHECK_ALLOC(*out);

	return 0;
}

void git_offmap_free(git_offmap *map)
{
	kh_destroy(off, map);
}

void git_offmap_clear(git_offmap *map)
{
	kh_clear(off, map);
}

size_t git_offmap_size(git_offmap *map)
{
	return kh_size(map);
}

void *git_offmap_get(git_offmap *map, const git_off_t key)
{
	size_t idx = git_offmap_lookup_index(map, key);
	if (!git_offmap_valid_index(map, idx) ||
	    !git_offmap_has_data(map, idx))
		return NULL;
	return kh_val(map, idx);
}

int git_offmap_set(git_offmap *map, const git_off_t key, void *value)
{
	size_t idx;
	int rval;

	idx = kh_put(off, map, key, &rval);
	if (rval < 0)
		return -1;

	if (rval == 0)
		kh_key(map, idx) = key;

	kh_val(map, idx) = value;

	return 0;
}

int git_offmap_delete(git_offmap *map, const git_off_t key)
{
	khiter_t idx = git_offmap_lookup_index(map, key);
	if (!git_offmap_valid_index(map, idx))
		return GIT_ENOTFOUND;
	git_offmap_delete_at(map, idx);
	return 0;
}

size_t git_offmap_lookup_index(git_offmap *map, const git_off_t key)
{
	return kh_get(off, map, key);
}

int git_offmap_valid_index(git_offmap *map, size_t idx)
{
	return idx != kh_end(map);
}

int git_offmap_exists(git_offmap *map, const git_off_t key)
{
	return kh_get(off, map, key) != kh_end(map);
}

int git_offmap_has_data(git_offmap *map, size_t idx)
{
	return kh_exist(map, idx);
}

git_off_t git_offmap_key_at(git_offmap *map, size_t idx)
{
	return kh_key(map, idx);
}

void *git_offmap_value_at(git_offmap *map, size_t idx)
{
	return kh_val(map, idx);
}

void git_offmap_set_value_at(git_offmap *map, size_t idx, void *value)
{
	kh_val(map, idx) = value;
}

void git_offmap_delete_at(git_offmap *map, size_t idx)
{
	kh_del(off, map, idx);
}

int git_offmap_put(git_offmap *map, const git_off_t key, int *err)
{
	return kh_put(off, map, key, err);
}

void git_offmap_insert(git_offmap *map, const git_off_t key, void *value, int *rval)
{
	khiter_t idx = kh_put(off, map, key, rval);

	if ((*rval) >= 0) {
		if ((*rval) == 0)
			kh_key(map, idx) = key;
		kh_val(map, idx) = value;
	}
}

size_t git_offmap_begin(git_offmap *map)
{
	GIT_UNUSED(map);
	return 0;
}

size_t git_offmap_end(git_offmap *map)
{
	return map->n_buckets;
}
