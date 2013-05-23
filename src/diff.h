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
	GIT_DIFFCAPS_TRUST_NANOSECS   = (1 << 5), /* use stat time nanoseconds */
};

enum {
	GIT_DIFF_FLAG__FREE_PATH  = (1 << 7),  /* `path` is allocated memory */
	GIT_DIFF_FLAG__FREE_DATA  = (1 << 8),  /* internal file data is allocated */
	GIT_DIFF_FLAG__UNMAP_DATA = (1 << 9),  /* internal file data is mmap'ed */
	GIT_DIFF_FLAG__NO_DATA    = (1 << 10), /* file data should not be loaded */

	GIT_DIFF_FLAG__TO_DELETE  = (1 << 16), /* delete entry during rename det. */
	GIT_DIFF_FLAG__TO_SPLIT   = (1 << 17), /* split entry during rename det. */
	GIT_DIFF_FLAG__IS_RENAME_TARGET = (1 << 18),
	GIT_DIFF_FLAG__IS_RENAME_SOURCE = (1 << 19),
	GIT_DIFF_FLAG__HAS_SELF_SIMILARITY = (1 << 20),
};

#define GIT_DIFF_FLAG__CLEAR_INTERNAL(F) (F) = ((F) & 0x00FFFF)

#define GIT_DIFF__VERBOSE  (1 << 30)

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

int git_diff_find_similar__hashsig_for_file(
	void **out, const git_diff_file *f, const char *path, void *p);

int git_diff_find_similar__hashsig_for_buf(
	void **out, const git_diff_file *f, const char *buf, size_t len, void *p);

void git_diff_find_similar__hashsig_free(void *sig, void *payload);

int git_diff_find_similar__calc_similarity(
	int *score, void *siga, void *sigb, void *payload);


#endif

