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

#define GIT__USE_OIDMAP \
	__KHASH_IMPL(oid, static kh_inline, const git_oid *, void *, 1, hash_git_oid, git_oid_equal)

#define git_oidmap_alloc() kh_init(oid)
#define git_oidmap_free(h) kh_destroy(oid,h), h = NULL

#endif
