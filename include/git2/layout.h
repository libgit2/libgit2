/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_layout_h__
#define INCLUDE_git_layout_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/layout.h
 * @brief Git repository layout helpers
 * @defgroup git_layout Git repository layout helpers
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * List of items which belong to the git repository layout
 */
typedef enum {
	GIT_REPOSITORY_ITEM_GITDIR,
	GIT_REPOSITORY_ITEM_WORKDIR,
	GIT_REPOSITORY_ITEM_COMMONDIR,
	GIT_REPOSITORY_ITEM_INDEX,
	GIT_REPOSITORY_ITEM_OBJECTS,
	GIT_REPOSITORY_ITEM_REFS,
	GIT_REPOSITORY_ITEM_PACKED_REFS,
	GIT_REPOSITORY_ITEM_REMOTES,
	GIT_REPOSITORY_ITEM_CONFIG,
	GIT_REPOSITORY_ITEM_INFO,
	GIT_REPOSITORY_ITEM_HOOKS,
	GIT_REPOSITORY_ITEM_LOGS,
	GIT_REPOSITORY_ITEM_MODULES,
	GIT_REPOSITORY_ITEM_WORKTREES,
	GIT_REPOSITORY_ITEM__LAST
} git_repository_item_t;

/**
 * Get the location of a specific repository file or directory
 *
 * This function will retrieve the path of a specific repository
 * item. It will thereby honor things like the repository's
 * common directory, gitdir, etc. In case a file path cannot
 * exist for a given item (e.g. the working directory of a bare
 * repository), GIT_ENOTFOUND is returned.
 *
 * @param out Buffer to store the path at
 * @param repo Repository to get path for
 * @param item The repository item for which to retrieve the path
 * @return 0, GIT_ENOTFOUND if the path cannot exist or an error code
 */
GIT_EXTERN(int) git_repository_item_path(git_buf *out, const git_repository *repo, git_repository_item_t item);

/** @} */
GIT_END_DECL
#endif
