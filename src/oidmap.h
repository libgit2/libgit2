/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_oidmap_h__
#define INCLUDE_oidmap_h__

#include "common.h"

#include "git2/oid.h"

/** A map with `git_oid`s as key. */
typedef struct kh_oid_s git_oidmap;

/**
 * Allocate a new OID map.
 *
 * @param out Pointer to the map that shall be allocated.
 * @return 0 on success, an error code if allocation has failed.
 */
int git_oidmap_new(git_oidmap **out);

/**
 * Free memory associated with the map.
 *
 * Note that this function will _not_ free values added to this
 * map.
 *
 * @param map Pointer to the map that is to be free'd. May be
 *            `NULL`.
 */
void git_oidmap_free(git_oidmap *map);

/**
 * Clear all entries from the map.
 *
 * This function will remove all entries from the associated map.
 * Memory associated with it will not be released, though.
 *
 * @param map Pointer to the map that shall be cleared. May be
 *            `NULL`.
 */
void git_oidmap_clear(git_oidmap *map);

/**
 * Return the number of elements in the map.
 *
 * @parameter map map containing the elements
 * @return number of elements in the map
 */
size_t git_oidmap_size(git_oidmap *map);

/**
 * Return value associated with the given key.
 *
 * @param map map to search key in
 * @param key key to search for
 * @return value associated with the given key or NULL if the key was not found
 */
void *git_oidmap_get(git_oidmap *map, const git_oid *key);

size_t git_oidmap_lookup_index(git_oidmap *map, const git_oid *key);
int git_oidmap_valid_index(git_oidmap *map, size_t idx);

int git_oidmap_exists(git_oidmap *map, const git_oid *key);
int git_oidmap_has_data(git_oidmap *map, size_t idx);

const git_oid *git_oidmap_key(git_oidmap *map, size_t idx);
void git_oidmap_set_key_at(git_oidmap *map, size_t idx, git_oid *key);
void *git_oidmap_value_at(git_oidmap *map, size_t idx);
void git_oidmap_set_value_at(git_oidmap *map, size_t idx, void *value);
void git_oidmap_delete_at(git_oidmap *map, size_t idx);

int git_oidmap_put(git_oidmap *map, const git_oid *key, int *err);
void git_oidmap_insert(git_oidmap *map, const git_oid *key, void *value, int *rval);
void git_oidmap_delete(git_oidmap *map, const git_oid *key);

size_t git_oidmap_begin(git_oidmap *map);
size_t git_oidmap_end(git_oidmap *map);

#define git_oidmap_foreach_value(h, vvar, code) { size_t __i;			\
	for (__i = git_oidmap_begin(h); __i != git_oidmap_end(h); ++__i) {	\
		if (!git_oidmap_has_data(h,__i)) continue;			\
		(vvar) = git_oidmap_value_at(h,__i);				\
		code;								\
	} }

#endif
