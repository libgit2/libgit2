/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_oidmap_h__
#define INCLUDE_oidmap_h__

#include "common.h"
#include "git2/oid.h"

#define kmalloc git__malloc
#define kcalloc git__calloc
#define krealloc git__realloc
#define kfree git__free
#include "khash.h"

__KHASH_TYPE(oid, const git_oid *, void *);
typedef khash_t(oid) git_oidmap;

GIT_INLINE(khint_t) hash_git_oid(const git_oid *oid)
{
	int i;
	khint_t h = 0;
	for (i = 0; i < 20; ++i)
		h = (h << 5) - h + oid->id[i];
	return h;
}

GIT_INLINE(int) hash_git_oid_equal(const git_oid *a, const git_oid *b)
{
	return (memcmp(a->id, b->id, sizeof(a->id)) == 0);
}

#define GIT__USE_OIDMAP \
	__KHASH_IMPL(oid, static inline, const git_oid *, void *, 1, hash_git_oid, hash_git_oid_equal)

#define git_oidmap_alloc() kh_init(oid)
#define git_oidmap_free(h) kh_destroy(oid,h), h = NULL

#endif
