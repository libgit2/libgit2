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

typedef struct kh_oid_s git_oidmap;

git_oidmap *git_oidmap_alloc(void);
void git_oidmap_free(git_oidmap *map);
void git_oidmap_clear(git_oidmap *map);

size_t git_oidmap_size(git_oidmap *map);

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
