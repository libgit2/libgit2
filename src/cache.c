/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "commit.h"
#include "thread-utils.h"
#include "cache.h"

int git_cache_init(git_cache *cache, size_t size, git_cached_obj_freeptr free_ptr)
{
	size_t i;

	if (size < 8)
		size = 8;

	/* round up size to closest power of 2 */
	size--;
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size |= size >> 16;

	cache->size_mask = size;
	cache->lru_count = 0;
	cache->free_obj = free_ptr;

	cache->nodes = git__malloc((size + 1) * sizeof(cache_node));
	if (cache->nodes == NULL)
		return GIT_ENOMEM;

	for (i = 0; i < (size + 1); ++i) {
		git_mutex_init(&cache->nodes[i].lock);
		cache->nodes[i].ptr = NULL;
	}

	return GIT_SUCCESS;
}

void git_cache_free(git_cache *cache)
{
	size_t i;

	for (i = 0; i < (cache->size_mask + 1); ++i) {
		if (cache->nodes[i].ptr)
			git_cached_obj_decref(cache->nodes[i].ptr, cache->free_obj);

		git_mutex_free(&cache->nodes[i].lock);
	}

	git__free(cache->nodes);
}

void *git_cache_get(git_cache *cache, const git_oid *oid)
{
	uint32_t hash;
	cache_node *node = NULL;
	void *result = NULL;

	memcpy(&hash, oid->id, sizeof(hash));
	node = &cache->nodes[hash & cache->size_mask];

	git_mutex_lock(&node->lock);
	{
		if (node->ptr && git_cached_obj_compare(node->ptr, oid) == 0) {
			git_cached_obj_incref(node->ptr);
			result = node->ptr;
		}
	}
	git_mutex_unlock(&node->lock);

	return result;
}

void *git_cache_try_store(git_cache *cache, void *entry)
{
	uint32_t hash;
	const git_oid *oid;
	cache_node *node = NULL;

	oid = &((git_cached_obj*)entry)->oid;
	memcpy(&hash, oid->id, sizeof(hash));
	node = &cache->nodes[hash & cache->size_mask];

	/* increase the refcount on this object, because
	 * the cache now owns it */
	git_cached_obj_incref(entry);
	git_mutex_lock(&node->lock);

	if (node->ptr == NULL) {
		node->ptr = entry;
	} else if (git_cached_obj_compare(node->ptr, oid) == 0) {
		git_cached_obj_decref(entry, cache->free_obj);
		entry = node->ptr;
	} else {
		git_cached_obj_decref(node->ptr, cache->free_obj);
		node->ptr = entry;
	}

	/* increase the refcount again, because we are
	 * returning it to the user */
	git_cached_obj_incref(entry);
	git_mutex_unlock(&node->lock);

	return entry;
}
