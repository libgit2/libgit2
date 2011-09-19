/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_status_h__
#define INCLUDE_git_status_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/status.h
 * @brief Git file status routines
 * @defgroup git_status Git file status routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

#define GIT_STATUS_CURRENT		0
/** Flags for index status */
#define GIT_STATUS_INDEX_NEW		(1 << 0)
#define GIT_STATUS_INDEX_MODIFIED (1 << 1)
#define GIT_STATUS_INDEX_DELETED (1 << 2)

/** Flags for worktree status */
#define GIT_STATUS_WT_NEW			(1 << 3)
#define GIT_STATUS_WT_MODIFIED	(1 << 4)
#define GIT_STATUS_WT_DELETED		(1 << 5)

// TODO Ignored files not handled yet
#define GIT_STATUS_IGNORED		(1 << 6)

/**
 * Gather file statuses and run a callback for each one.
 *
 * The callback is passed the path of the file, the status and the data pointer
 * passed to this function. If the callback returns something other than
 * GIT_SUCCESS, this function will return that value.
 *
 * @param repo a repository object
 * @param callback the function to call on each file
 * @return GIT_SUCCESS or the return value of the callback which did not return GIT_SUCCESS
 */
GIT_EXTERN(int) git_status_foreach(git_repository *repo, int (*callback)(const char *, unsigned int, void *), void *payload);

/**
 * Get file status for a single file
 *
 * @param status_flags the status value
 * @param repo a repository object
 * @param path the file to retrieve status for, rooted at the repo's workdir
 * @return GIT_EINVALIDPATH when `path` points at a folder, GIT_ENOTFOUND when
 *		the file doesn't exist in any of HEAD, the index or the worktree,
 *		GIT_SUCCESS otherwise
 */
GIT_EXTERN(int) git_status_file(unsigned int *status_flags, git_repository *repo, const char *path);

/** @} */
GIT_END_DECL
#endif
