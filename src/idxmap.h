/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_idxmap_h__
#define INCLUDE_idxmap_h__

#include "common.h"

#include "git2/index.h"

/** A map with `git_index_entry`s as key. */
typedef struct kh_idx_s git_idxmap;
/** A map with case-insensitive `git_index_entry`s as key */
typedef struct kh_idxicase_s git_idxmap_icase;

/**
 * Allocate a new index entry map.
 *
 * @param out Pointer to the map that shall be allocated.
 * @return 0 on success, an error code if allocation has failed.
 */
int git_idxmap_new(git_idxmap **out);

/**
 * Allocate a new case-insensitive index entry map.
 *
 * @param out Pointer to the map that shall be allocated.
 * @return 0 on success, an error code if allocation has failed.
 */
int git_idxmap_icase_new(git_idxmap_icase **out);

/**
 * Free memory associated with the map.
 *
 * Note that this function will _not_ free values added to this
 * map.
 *
 * @param map Pointer to the map that is to be free'd. May be
 *            `NULL`.
 */
void git_idxmap_free(git_idxmap *map);

/**
 * Free memory associated with the map.
 *
 * Note that this function will _not_ free values added to this
 * map.
 *
 * @param map Pointer to the map that is to be free'd. May be
 *            `NULL`.
 */
void git_idxmap_icase_free(git_idxmap_icase *map);

/**
 * Clear all entries from the map.
 *
 * This function will remove all entries from the associated map.
 * Memory associated with it will not be released, though.
 *
 * @param map Pointer to the map that shall be cleared. May be
 *            `NULL`.
 */
void git_idxmap_clear(git_idxmap *map);

/**
 * Clear all entries from the map.
 *
 * This function will remove all entries from the associated map.
 * Memory associated with it will not be released, though.
 *
 * @param map Pointer to the map that shall be cleared. May be
 *            `NULL`.
 */
void git_idxmap_icase_clear(git_idxmap_icase *map);

void git_idxmap_insert(git_idxmap *map, const git_index_entry *key, void *value, int *rval);
void git_idxmap_icase_insert(git_idxmap_icase *map, const git_index_entry *key, void *value, int *rval);

size_t git_idxmap_lookup_index(git_idxmap *map, const git_index_entry *key);
size_t git_idxmap_icase_lookup_index(git_idxmap_icase *map, const git_index_entry *key);
void *git_idxmap_value_at(git_idxmap *map, size_t idx);
int git_idxmap_valid_index(git_idxmap *map, size_t idx);
int git_idxmap_has_data(git_idxmap *map, size_t idx);

void git_idxmap_resize(git_idxmap *map, size_t size);
void git_idxmap_icase_resize(git_idxmap_icase *map, size_t size);

void git_idxmap_delete_at(git_idxmap *map, size_t idx);
void git_idxmap_icase_delete_at(git_idxmap_icase *map, size_t idx);

void git_idxmap_delete(git_idxmap *map, const git_index_entry *key);
void git_idxmap_icase_delete(git_idxmap_icase *map, const git_index_entry *key);

#endif
