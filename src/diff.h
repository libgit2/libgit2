/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_diff_h__
#define INCLUDE_diff_h__

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

#define MAX_DIFF_FILESIZE 0x20000000

struct git_diff_list {
	git_refcount     rc;
	git_repository   *repo;
	git_diff_options opts;
	git_vector       pathspec;
	git_vector       deltas;    /* vector of git_diff_file_delta */
	git_pool pool;
	git_iterator_type_t old_src;
	git_iterator_type_t new_src;
	uint32_t diffcaps;
};

extern void git_diff__cleanup_modes(
	uint32_t diffcaps, uint32_t *omode, uint32_t *nmode);

/**
 * Return the maximum possible number of files in the diff.
 *
 * NOTE: This number has to be treated as an upper bound on the number of
 * files that have changed if the diff is with the working directory.
 *
 * Why?! For efficiency, we defer loading the file contents as long as
 * possible, so if a file has been "touched" in the working directory and
 * then reverted to the original content, it may get stored in the diff list
 * as MODIFIED along with a flag that the status should be reconfirmed when
 * it is actually loaded into memory.  When that load happens, it could get
 * flipped to UNMODIFIED. If unmodified files are being skipped, then the
 * iterator will skip that file and this number may be too high.
 *
 * This behavior is true of `git_diff_foreach` as well, but the only
 * implication there is that the `progress` value would not advance evenly.
 *
 * @param iterator The iterator object
 * @return The maximum number of files to be iterated over
 */
int git_diff_iterator__max_files(git_diff_iterator *iterator);

#endif

