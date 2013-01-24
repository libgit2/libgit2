/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_index_map_h__
#define INCLUDE_index_map_h__

#include "common.h"

#define kmalloc git__malloc
#define kcalloc git__calloc
#define krealloc git__realloc
#define kfree git__free
#include "khash.h"

__KHASH_TYPE(index_entry, const git_index_entry *, git_index_entry *);
typedef khash_t(index_entry) git_index_entry_map;

GIT_INLINE(khint_t) hash_git_index_entry(const git_index_entry *entry)
{
	const char *s = entry->path;
	khint_t c;
	khint_t h = git_index_entry_stage(entry);
	assert(s);
	while ((c = *s++))
		h = c + (h << 6) + (h << 16) - h;
	return h;
}

GIT_INLINE(khint_t) git_index_entry_equal(const git_index_entry *a, const git_index_entry *b)
{
	return git_index_entry_stage(a) == git_index_entry_stage(a)
		&& ((!a->path && !b->path) || (a->path && b->path && !strcmp(a->path, b->path)));
}

#define GIT__USE_INDEX_ENTRY_MAP \
	__KHASH_IMPL(index_entry, static kh_inline, const git_index_entry *, git_index_entry *, 0, hash_git_index_entry, git_index_entry_equal)

#define git_index_entry_map_alloc() kh_init(index_entry)
#define git_index_entry_map_free(h) kh_destroy(index_entry,h), h = NULL

#endif
