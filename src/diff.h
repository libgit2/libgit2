/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_diff_h__
#define INCLUDE_diff_h__

#include "git2/diff.h"
#include "git2/oid.h"

#include <stdio.h>
#include "vector.h"
#include "buffer.h"
#include "iterator.h"
#include "repository.h"
#include "pool.h"

#define DIFF_OLD_PREFIX_DEFAULT "a/"
#define DIFF_NEW_PREFIX_DEFAULT "b/"

enum {
	GIT_DIFFCAPS_HAS_SYMLINKS     = (1 << 0), /* symlinks on platform? */
	GIT_DIFFCAPS_ASSUME_UNCHANGED = (1 << 1), /* use stat? */
	GIT_DIFFCAPS_TRUST_MODE_BITS  = (1 << 2), /* use st_mode? */
	GIT_DIFFCAPS_TRUST_CTIME      = (1 << 3), /* use st_ctime? */
	GIT_DIFFCAPS_USE_DEV          = (1 << 4), /* use st_dev? */
};

enum {
	GIT_DIFF_FLAG__FREE_PATH  = (1 << 7),  /* `path` is allocated memory */
	GIT_DIFF_FLAG__FREE_DATA  = (1 << 8),  /* internal file data is allocated */
	GIT_DIFF_FLAG__UNMAP_DATA = (1 << 9),  /* internal file data is mmap'ed */
	GIT_DIFF_FLAG__NO_DATA    = (1 << 10), /* file data should not be loaded */
	GIT_DIFF_FLAG__TO_DELETE  = (1 << 11), /* delete entry during rename det. */
	GIT_DIFF_FLAG__TO_SPLIT   = (1 << 12), /* split entry during rename det. */
};

struct git_diff_list {
	git_refcount     rc;
	git_repository   *repo;
	git_diff_options opts;
	git_vector       pathspec;
	git_vector       deltas;    /* vector of git_diff_delta */
	git_pool pool;
	git_iterator_type_t old_src;
	git_iterator_type_t new_src;
	uint32_t diffcaps;

	int (*strcomp)(const char *, const char *);
	int (*strncomp)(const char *, const char *, size_t);
	int (*pfxcomp)(const char *str, const char *pfx);
	int (*entrycomp)(const void *a, const void *b);
};

extern void git_diff__cleanup_modes(
	uint32_t diffcaps, uint32_t *omode, uint32_t *nmode);

extern void git_diff_list_addref(git_diff_list *diff);

extern int git_diff_delta__cmp(const void *a, const void *b);

extern bool git_diff_delta__should_skip(
	const git_diff_options *opts, const git_diff_delta *delta);

extern int git_diff__oid_for_file(
	git_repository *, const char *, uint16_t, git_off_t, git_oid *);

extern int git_diff__from_iterators(
	git_diff_list **diff_ptr,
	git_repository *repo,
	git_iterator *old_iter,
	git_iterator *new_iter,
	const git_diff_options *opts);

#endif

