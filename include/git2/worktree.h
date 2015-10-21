/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_worktree_h__
#define INCLUDE_git_worktree_h__

#include "common.h"
#include "types.h"
#include "strarray.h"

/**
 * @file git2/worktrees.h
 * @brief Git worktree related functions
 * @defgroup git_commit Git worktree related functions
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * List names of linked working trees
 *
 * The returned list should be released with `git_strarray_free`
 * when no longer needed.
 *
 * @param out pointer to the array of working tree names
 * @param repo the repo to use when listing working trees
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_worktree_list(git_strarray *out, git_repository *repo);

/** @} */
GIT_END_DECL
#endif
