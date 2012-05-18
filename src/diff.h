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
	GIT_DIFFCAPS_TRUST_EXEC_BIT   = (1 << 2), /* use st_mode exec bit? */
	GIT_DIFFCAPS_TRUST_CTIME      = (1 << 3), /* use st_ctime? */
	GIT_DIFFCAPS_USE_DEV          = (1 << 4), /* use st_dev? */
};

struct git_diff_list {
	git_repository   *repo;
	git_diff_options opts;
	git_vector       pathspec;
	git_vector       deltas;    /* vector of git_diff_file_delta */
	git_pool pool;
	git_iterator_type_t old_src;
	git_iterator_type_t new_src;
	uint32_t diffcaps;
};

#endif

