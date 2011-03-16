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
#include "repository.h"
#include "commit.h"
#include "thread-utils.h"
#include "cache.h"

#define GIT_CACHE_OPENADR 3


GIT_INLINE(int) cached_obj_compare(git_cached_obj *obj, const git_oid *oid)
{
	return git_oid_cmp(&obj->oid, oid);
}

GIT_INLINE(void) cached_obj_incref(git_cached_obj *obj)
{
	git_atomic_inc(&obj->refcount);
}

GIT_INLINE(void) cached_obj_decref(git_cached_obj *obj, git_cached_obj_freeptr free_obj)
{
	if (git_atomic_dec(&obj->refcount) == 0)
		free_obj(obj);
}


void git_cache_init(git_cache *cache, size_t size, git_cached_obj_freeptr free_ptr)
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

	for (i = 0; i < (size + 1); ++i) {
		git_mutex_init(&cache->nodes[i].lock);
		cache->nodes[i].ptr = NULL;
		cache->nodes[i].lru = 0;
	}
}

void git_cache_free(git_cache *cache)
{
	size_t i;

	for (i = 0; i < (cache->size_mask + 1); ++i) {
		cached_obj_decref(cache->nodes[i].ptr, cache->free_obj);
		git_mutex_free(&cache->nodes[i].lock);
	}

	free(cache->nodes);
}

void *git_cache_get(git_cache *cache, const git_oid *oid)
{
	const uint32_t *hash;
	size_t i, pos, found = 0;
	cache_node *node;

	hash = (const uint32_t *)oid->id;

	for (i = 0; !found && i < GIT_CACHE_OPENADR; ++i) {
		pos = hash[i] & cache->size_mask;
		node = &cache->nodes[pos];

		git_mutex_lock(&node->lock);
		{
			if (cached_obj_compare(node->ptr, oid) == 0) {
				cached_obj_incref(node->ptr);
				node->lru = ++cache->lru_count;
				found = 1;
			}
		}
		git_mutex_unlock(&node->lock);
	}


	return found ? node->ptr : NULL;
}

void *git_cache_try_store(git_cache *cache, void *entry)
{
	cache_node *nodes[GIT_CACHE_OPENADR], *lru_node;
	const uint32_t *hash;
	const git_oid *oid;
	size_t i, stored = 0;

	oid = &((git_cached_obj*)entry)->oid;
	hash = (const uint32_t *)oid->id;

	/* increase the refcount on this object, because
	 * the cache now owns it */
	cached_obj_incref(entry);

	for (i = 0; i < GIT_CACHE_OPENADR; ++i) {
		size_t pos = hash[i] & cache->size_mask;

		nodes[i] = &cache->nodes[pos];
		git_mutex_lock(&nodes[i]->lock);
	}

	lru_node = nodes[0];

	for (i = 0; !stored && entry && i < GIT_CACHE_OPENADR; ++i) {

		if (nodes[i]->ptr == NULL) {
			nodes[i]->ptr = entry;
			nodes[i]->lru = ++cache->lru_count;
			stored = 1;
		} else if (cached_obj_compare(nodes[i]->ptr, oid) == 0) {
			cached_obj_decref(entry, cache->free_obj);
			entry = nodes[i]->ptr;
			stored = 1;
		}

		if (nodes[i]->lru < lru_node->lru)
			lru_node = nodes[i];
	}

	if (!stored) {
		void *old_entry = lru_node->ptr;
		assert(old_entry);

		cached_obj_decref(old_entry, cache->free_obj);
		lru_node->ptr = entry;
		lru_node->lru = ++cache->lru_count;
	}

	/* increase the refcount again, because we are
	 * returning it to the user */
	cached_obj_incref(entry);

	for (i = 0; i < GIT_CACHE_OPENADR; ++i)
		git_mutex_unlock(&nodes[i]->lock);

	return entry;
}
