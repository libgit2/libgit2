/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_strmap_h__
#define INCLUDE_strmap_h__

#include "common.h"

typedef struct kh_str_s git_strmap;

int git_strmap_alloc(git_strmap **map);
void git_strmap_free(git_strmap *map);
void git_strmap_clear(git_strmap *map);

size_t git_strmap_num_entries(git_strmap *map);

size_t git_strmap_lookup_index(git_strmap *map, const char *key);
int git_strmap_valid_index(git_strmap *map, size_t idx);

int git_strmap_exists(git_strmap *map, const char *key);
int git_strmap_has_data(git_strmap *map, size_t idx);

const char *git_strmap_key(git_strmap *map, size_t idx);
void git_strmap_set_key_at(git_strmap *map, size_t idx, char *key);
void *git_strmap_value_at(git_strmap *map, size_t idx);
void git_strmap_set_value_at(git_strmap *map, size_t idx, void *value);
void git_strmap_delete_at(git_strmap *map, size_t idx);

int git_strmap_put(git_strmap *map, const char *key, int *err);
void git_strmap_insert(git_strmap *map, const char *key, void *value, int *rval);
void git_strmap_delete(git_strmap *map, const char *key);

#define git_strmap_foreach(h, kvar, vvar, code) { size_t __i;			\
	for (__i = git_strmap_begin(h); __i != git_strmap_end(h); ++__i) {	\
		if (!git_strmap_has_data(h,__i)) continue;			\
		(kvar) = git_strmap_key(h,__i);					\
		(vvar) = git_strmap_value_at(h,__i);				\
		code;								\
	} }

#define git_strmap_foreach_value(h, vvar, code) { size_t __i;			\
	for (__i = git_strmap_begin(h); __i != git_strmap_end(h); ++__i) {	\
		if (!git_strmap_has_data(h,__i)) continue;			\
		(vvar) = git_strmap_value_at(h,__i);				\
		code;								\
	} }

size_t git_strmap_begin(git_strmap *map);
size_t git_strmap_end(git_strmap *map);

int git_strmap_next(
	void **data,
	size_t *iter,
	git_strmap *map);

#endif
