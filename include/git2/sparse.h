/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_sparse_h__
#define INCLUDE_git_sparse_h__

#include "common.h"
#include "types.h"

GIT_BEGIN_DECL

/**
 * Test if the sparse-checkout rules apply to a given path.
 *
 * This function checks the sparse-checkout rules to see if they would apply to the
 * given path. This indicates if the path would be included on checkout.
 *
 * @param checkout boolean returning 1 if the sparse-checkout rules apply (the file will be checked out), 0 if they do not
 * @param repo a repository object
 * @param path the path to check sparse-checkout rules for, relative to the repo's workdir.
 * @return 0 if sparse-checkout rules could be processed for the path (regardless
 *         of whether it exists or not), or an error < 0 if they could not.
 */
GIT_EXTERN(int) git_sparse_check_path(
	int *checkout,
	git_repository *repo,
	const char *path);

GIT_END_DECL

#endif
