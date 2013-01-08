/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_cache_h__
#define INCLUDE_cache_h__

#include "git2/common.h"
#include "git2/oid.h"
#include "git2/odb.h"

#include "thread-utils.h"

#define GIT_DEFAULT_CACHE_SIZE 128

typedef void (*git_cached_obj_freeptr)(void *);

typedef struct {
	git_oid oid;
	git_atomic refcount;
} git_cached_obj;

typedef struct {
	git_cached_obj **nodes;
	git_mutex lock;

	unsigned int lru_count;
	size_t size_mask;
	git_cached_obj_freeptr free_obj;
} git_cache;

int git_cache_init(git_cache *cache, size_t size, git_cached_obj_freeptr free_ptr);
void git_cache_free(git_cache *cache);

void *git_cache_try_store(git_cache *cache, void *entry);
void *git_cache_get(git_cache *cache, const git_oid *oid);

GIT_INLINE(void) git_cached_obj_incref(void *_obj)
{
	git_cached_obj *obj = _obj;
	git_atomic_inc(&obj->refcount);
}

GIT_INLINE(void) git_cached_obj_decref(void *_obj, git_cached_obj_freeptr free_obj)
{
	git_cached_obj *obj = _obj;

	if (git_atomic_dec(&obj->refcount) == 0)
		free_obj(obj);
}

#endif
