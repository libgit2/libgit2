/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_idxmap_h__
#define INCLUDE_idxmap_h__

#include "common.h"

#include <ctype.h>
#include "git2/index.h"

#define kmalloc git__malloc
#define kcalloc git__calloc
#define krealloc git__realloc
#define kreallocarray git__reallocarray
#define kfree git__free
#include "khash.h"

typedef khash git_idxmap;

typedef khiter_t git_idxmap_iter;

int git_idxmap_alloc(git_idxmap **map, bool ignore_case);
void git_idxmap_set_ignore_case(git_idxmap *map, bool ignore_case);
void git_idxmap_insert(git_idxmap *map, const git_index_entry *key, void *value, int *rval);

size_t git_idxmap_lookup_index(git_idxmap *map, const git_index_entry *key);
void *git_idxmap_value_at(git_idxmap *map, size_t idx);
int git_idxmap_valid_index(git_idxmap *map, size_t idx);
int git_idxmap_has_data(git_idxmap *map, size_t idx);

void git_idxmap_resize(git_idxmap *map, size_t size);
void git_idxmap_free(git_idxmap *map);
void git_idxmap_clear(git_idxmap *map);

void git_idxmap_delete_at(git_idxmap *map, size_t idx);

void git_idxmap_delete(git_idxmap *map, const git_index_entry *key);

#define git_idxmap_begin		kh_begin
#define git_idxmap_end		kh_end

#endif
