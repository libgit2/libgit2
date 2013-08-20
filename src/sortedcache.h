/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sorted_cache_h__
#define INCLUDE_sorted_cache_h__

#include "util.h"
#include "fileops.h"
#include "vector.h"
#include "thread-utils.h"
#include "pool.h"
#include "strmap.h"

/*
 * The purpose of this data structure is to cache the parsed contents of a
 * file where each item in the file can be identified by a key string and
 * you want to both look them up by name and traverse them in sorted
 * order.  Each item is assumed to itself end in a GIT_FLEX_ARRAY.
 */

typedef void (*git_sortedcache_free_item_fn)(void *payload, void *item);

typedef struct {
	git_refcount rc;
	git_mutex    lock;
	size_t       item_path_offset;
	git_sortedcache_free_item_fn free_item;
	void         *free_item_payload;
	git_pool     pool;
	git_vector   items;
	git_strmap   *map;
	git_futils_filestamp stamp;
	char         path[GIT_FLEX_ARRAY];
} git_sortedcache;

/* create a new sortedcache
 *
 * even though every sortedcache stores items with a GIT_FLEX_ARRAY at
 * the end containing their key string, you have to provide the item_cmp
 * sorting function because the sorting function doesn't get a payload
 * and therefore can't know the offset to the item key string. :-(
 */
int git_sortedcache_new(
	git_sortedcache **out,
	size_t item_path_offset, /* use offsetof() macro */
	git_sortedcache_free_item_fn free_item,
	void *free_item_payload,
	git_vector_cmp item_cmp,
	const char *path);

/* copy a sorted cache
 *
 * - copy_item can be NULL to memcpy
 * - locks src while copying
 */
int git_sortedcache_copy(
	git_sortedcache **out,
	git_sortedcache *src,
	int (*copy_item)(void *payload, void *tgt_item, void *src_item),
	void *payload);

/* free sorted cache (first calling free_item callbacks) */
void git_sortedcache_free(git_sortedcache *sc);

/* increment reference count */
void git_sortedcache_incref(git_sortedcache *sc);

/* release all items in sorted cache - lock during clear if lock is true */
void git_sortedcache_clear(git_sortedcache *sc, bool lock);

/* check file stamp to see if reload is required */
bool git_sortedcache_out_of_date(git_sortedcache *sc);

/* lock sortedcache while making modifications */
int git_sortedcache_lock(git_sortedcache *sc);

/* unlock sorted cache when done with modifications */
int git_sortedcache_unlock(git_sortedcache *sc);

/* if the file has changed, lock cache and load file contents into buf;
 * @return 0 if up-to-date, 1 if out-of-date, <0 on error
 */
int git_sortedcache_lockandload(git_sortedcache *sc, git_buf *buf);

/* find and/or insert item, returning pointer to item data - lock first */
int git_sortedcache_upsert(
	void **out, git_sortedcache *sc, const char *key);

/* lookup item by key */
void *git_sortedcache_lookup(const git_sortedcache *sc, const char *key);

/* find out how many items are in the cache */
size_t git_sortedcache_entrycount(const git_sortedcache *sc);

/* lookup item by index */
void *git_sortedcache_entry(const git_sortedcache *sc, size_t pos);

#endif
