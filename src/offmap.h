/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_offmap_h__
#define INCLUDE_offmap_h__

#include "common.h"

#include "git2/types.h"

/** A map with `git_off_t`s as key. */
typedef struct kh_off_s git_offmap;

/**
 * Allocate a new `git_off_t` map.
 *
 * @param out Pointer to the map that shall be allocated.
 * @return 0 on success, an error code if allocation has failed.
 */
int git_offmap_new(git_offmap **out);

/**
 * Free memory associated with the map.
 *
 * Note that this function will _not_ free values added to this
 * map.
 *
 * @param map Pointer to the map that is to be free'd. May be
 *            `NULL`.
 */
void git_offmap_free(git_offmap *map);

/**
 * Clear all entries from the map.
 *
 * This function will remove all entries from the associated map.
 * Memory associated with it will not be released, though.
 *
 * @param map Pointer to the map that shall be cleared. May be
 *            `NULL`.
 */
void git_offmap_clear(git_offmap *map);

/**
 * Return the number of elements in the map.
 *
 * @parameter map map containing the elements
 * @return number of elements in the map
 */
size_t git_offmap_size(git_offmap *map);

/**
 * Return value associated with the given key.
 *
 * @param map map to search key in
 * @param key key to search for
 * @return value associated with the given key or NULL if the key was not found
 */
void *git_offmap_get(git_offmap *map, const git_off_t key);

/**
 * Set the entry for key to value.
 *
 * If the map has no corresponding entry for the given key, a new
 * entry will be created with the given value. If an entry exists
 * already, its value will be updated to match the given value.
 *
 * @param map map to create new entry in
 * @param key key to set
 * @param value value to associate the key with; may be NULL
 * @return zero if the key was successfully set, a negative error
 *         code otherwise
 */
int git_offmap_set(git_offmap *map, const git_off_t key, void *value);

size_t git_offmap_lookup_index(git_offmap *map, const git_off_t key);
int git_offmap_valid_index(git_offmap *map, size_t idx);

int git_offmap_exists(git_offmap *map, const git_off_t key);
int git_offmap_has_data(git_offmap *map, size_t idx);

git_off_t git_offmap_key_at(git_offmap *map, size_t idx);
void *git_offmap_value_at(git_offmap *map, size_t idx);
void git_offmap_set_value_at(git_offmap *map, size_t idx, void *value);
void git_offmap_delete_at(git_offmap *map, size_t idx);

int git_offmap_put(git_offmap *map, const git_off_t key, int *err);
void git_offmap_insert(git_offmap *map, const git_off_t key, void *value, int *rval);
void git_offmap_delete(git_offmap *map, const git_off_t key);

size_t git_offmap_begin(git_offmap *map);
size_t git_offmap_end(git_offmap *map);

#define git_offmap_foreach(h, kvar, vvar, code) { size_t __i;			\
	for (__i = git_offmap_begin(h); __i != git_offmap_end(h); ++__i) {	\
		if (!git_offmap_has_data(h,__i)) continue;			\
		(kvar) = git_offmap_key_at(h,__i);				\
		(vvar) = git_offmap_value_at(h,__i);				\
		code;								\
	} }

#define git_offmap_foreach_value(h, vvar, code) { size_t __i;			\
	for (__i = git_offmap_begin(h); __i != git_offmap_end(h); ++__i) {	\
		if (!git_offmap_has_data(h,__i)) continue;			\
		(vvar) = git_offmap_value_at(h,__i);				\
		code;								\
	} }

#endif
