/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef ALLOCMAP_H
#define ALLOCMAP_H

#include "common.h"
#include "alloc.h"

#define kmalloc  malloc
#define kcalloc  calloc
#define krealloc realloc
#define kfree    free

#include "khash.h"

#if (SIZE_MAX == UINT_MAX)
# define kh_alloc_hash_func(k) (khint32_t)((size_t)k)
#elif (SIZE_MAX == ULONG_MAX)
# define kh_alloc_hash_func(k) (khint32_t)(((size_t)key)>>33^((size_t)key)^((size_t)key)<<11)
#else
# error unknown architecture
#endif

#define kh_alloc_hash_equal(a, b) (((size_t)a) == ((size_t)b))

__KHASH_TYPE(alloc, void *, size_t);
typedef khash_t(alloc) git_allocmap;
typedef khiter_t git_allocmap_iter;

#define GIT__USE_ALLOCMAP \
	__KHASH_IMPL(alloc, static kh_inline, void *, size_t, 1, kh_alloc_hash_func, kh_alloc_hash_equal)

#define git_allocmap_alloc(hp) \
	((*(hp) = kh_init(alloc)) == NULL) ? giterr_set_oom(), -1 : 0

#define git_allocmap_free(h)  kh_destroy(alloc, h), h = NULL
#define git_allocmap_clear(h) kh_clear(alloc, h)

#define git_allocmap_num_entries(h) kh_size(h)

#define git_allocmap_lookup_index(h, k)  kh_get(alloc, h, k)
#define git_allocmap_valid_index(h, idx) (idx != kh_end(h))

#define git_allocmap_exists(h, k) (kh_get(alloc, h, k) != kh_end(h))
#define git_allocmap_has_data(h, idx) kh_exist(h, idx)

#define git_allocmap_key(h, idx)             kh_key(h, idx)
#define git_allocmap_value_at(h, idx)        kh_val(h, idx)
#define git_allocmap_set_value_at(h, idx, v) kh_val(h, idx) = v
#define git_allocmap_delete_at(h, idx)       kh_del(alloc, h, idx)

#define git_allocmap_insert(h, key, val, rval) do { \
	khiter_t __pos = kh_put(alloc, h, key, &rval); \
	if (rval >= 0) { \
		if (rval == 0) kh_key(h, __pos) = key; \
		kh_val(h, __pos) = val; \
	} } while (0)

#define git_allocmap_insert2(h, key, val, oldv, rval) do { \
	khiter_t __pos = kh_put(alloc, h, key, &rval); \
	if (rval >= 0) { \
		if (rval == 0) { \
			oldv = kh_val(h, __pos); \
			kh_key(h, __pos) = key; \
		} else { oldv = NULL; } \
		kh_val(h, __pos) = val; \
	} } while (0)

#define git_allocmap_delete(h, key) do { \
	khiter_t __pos = git_allocmap_lookup_index(h, key); \
	if (git_allocmap_valid_index(h, __pos)) \
		git_allocmap_delete_at(h, __pos); } while (0)

#define git_allocmap_foreach		kh_foreach
#define git_allocmap_foreach_value	kh_foreach_value

#define git_allocmap_begin		kh_begin
#define git_allocmap_end		kh_end

int git_allocmap_next(
	void **data,
	git_allocmap_iter* iter,
	git_allocmap *map);

#endif
