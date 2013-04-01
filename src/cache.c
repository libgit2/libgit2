/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "commit.h"
#include "thread-utils.h"
#include "util.h"
#include "cache.h"
#include "odb.h"
#include "object.h"
#include "git2/oid.h"

GIT__USE_OIDMAP

int git_cache_init(git_cache *cache)
{
	cache->map = git_oidmap_alloc();
	git_mutex_init(&cache->lock);
	return 0;
}

void git_cache_free(git_cache *cache)
{
	git_oidmap_free(cache->map);
	git_mutex_free(&cache->lock);
}

static bool cache_should_store(git_cached_obj *entry)
{
	return true;
}

static void *cache_get(git_cache *cache, const git_oid *oid, unsigned int flags)
{
	khiter_t pos;
	git_cached_obj *entry = NULL;

	if (git_mutex_lock(&cache->lock) < 0)
		return NULL;

	pos = kh_get(oid, cache->map, oid);
	if (pos != kh_end(cache->map)) {
		entry = kh_val(cache->map, pos);

		if (flags && entry->flags != flags) {
			entry = NULL;
		} else {
			git_cached_obj_incref(entry);
		}
	}

	git_mutex_unlock(&cache->lock);

	return entry;
}

static void *cache_store(git_cache *cache, git_cached_obj *entry)
{
	khiter_t pos;

	git_cached_obj_incref(entry);

	if (!cache_should_store(entry))
		return entry;

	if (git_mutex_lock(&cache->lock) < 0)
		return entry;

	pos = kh_get(oid, cache->map, &entry->oid);

	/* not found */
	if (pos == kh_end(cache->map)) {
		int rval;

		pos = kh_put(oid, cache->map, &entry->oid, &rval);
		if (rval >= 0) {
			kh_key(cache->map, pos) = &entry->oid;
			kh_val(cache->map, pos) = entry;
			git_cached_obj_incref(entry);
		}
	}
	/* found */
	else {
		git_cached_obj *stored_entry = kh_val(cache->map, pos);

		if (stored_entry->flags == entry->flags) {
			git_cached_obj_decref(entry);
			git_cached_obj_incref(stored_entry);
			entry = stored_entry;
		} else if (stored_entry->flags == GIT_CACHE_STORE_RAW &&
			entry->flags == GIT_CACHE_STORE_PARSED) {
			git_cached_obj_decref(stored_entry);
			git_cached_obj_incref(entry);

			kh_key(cache->map, pos) = &entry->oid;
			kh_val(cache->map, pos) = entry;
		} else {
			/* NO OP */
		}
	}

	git_mutex_unlock(&cache->lock);
	return entry;
}

void *git_cache_store_raw(git_cache *cache, git_odb_object *entry)
{
	entry->cached.flags = GIT_CACHE_STORE_RAW;
	return cache_store(cache, (git_cached_obj *)entry);
}

void *git_cache_store_parsed(git_cache *cache, git_object *entry)
{
	entry->cached.flags = GIT_CACHE_STORE_PARSED;
	return cache_store(cache, (git_cached_obj *)entry);
}

git_odb_object *git_cache_get_raw(git_cache *cache, const git_oid *oid)
{
	return cache_get(cache, oid, GIT_CACHE_STORE_RAW);
}

git_object *git_cache_get_parsed(git_cache *cache, const git_oid *oid)
{
	return cache_get(cache, oid, GIT_CACHE_STORE_PARSED);
}

void *git_cache_get_any(git_cache *cache, const git_oid *oid)
{
	return cache_get(cache, oid, GIT_CACHE_STORE_ANY);
}

void git_cached_obj_decref(void *_obj)
{
	git_cached_obj *obj = _obj;

	if (git_atomic_dec(&obj->refcount) == 0) {
		switch (obj->flags) {
		case GIT_CACHE_STORE_RAW:
			git_odb_object__free(_obj);
			break;

		case GIT_CACHE_STORE_PARSED:
			git_object__free(_obj);
			break;

		default:
			git__free(_obj);
			break;
		}
	}
}
