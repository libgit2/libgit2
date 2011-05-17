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

	free(cache->nodes);
}

void *git_cache_get(git_cache *cache, const git_oid *oid)
{
	const uint32_t *hash;
	cache_node *node = NULL;
	void *result = NULL;

	hash = (const uint32_t *)oid->id;
	node = &cache->nodes[hash[0] & cache->size_mask];

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
	const uint32_t *hash;
	const git_oid *oid;
	cache_node *node = NULL;

	oid = &((git_cached_obj*)entry)->oid;
	hash = (const uint32_t *)oid->id;
	node = &cache->nodes[hash[0] & cache->size_mask];

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
