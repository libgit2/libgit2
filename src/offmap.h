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

typedef struct kh_off_s git_offmap;

git_offmap *git_offmap_alloc(void);
void git_offmap_free(git_offmap *map);
void git_offmap_clear(git_offmap *map);

size_t git_offmap_num_entries(git_offmap *map);

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
