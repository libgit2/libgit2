/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

#define GIT_STATUS_CURRENT        0
/** Flags for index status */
#define GIT_STATUS_INDEX_NEW      (1 << 0)
#define GIT_STATUS_INDEX_MODIFIED (1 << 1)
#define GIT_STATUS_INDEX_DELETED  (1 << 2)

/** Flags for worktree status */
#define GIT_STATUS_WT_NEW         (1 << 3)
#define GIT_STATUS_WT_MODIFIED    (1 << 4)
#define GIT_STATUS_WT_DELETED     (1 << 5)

// TODO Ignored files not handled yet
#define GIT_STATUS_IGNORED        (1 << 6)

/**
 * Gather file statuses and run a callback for each one.
 *
 * The callback is passed the path of the file, the status and the data pointer
 * passed to this function. If the callback returns something other than
 * GIT_SUCCESS, this function will return that value.
 *
 * @param repo a repository object
 * @param callback the function to call on each file
 * @return GIT_SUCCESS or the return value of the callback which did not return 0;
 */
GIT_EXTERN(int) git_status_foreach(git_repository *repo, int (*callback)(const char *, unsigned int, void *), void *payload);

/**
 * Get file status for a single file
 *
 * @param status_flags the status value
 * @param repo a repository object
 * @param path the file to retrieve status for, rooted at the repo's workdir
 * @return GIT_SUCCESS
 */
GIT_EXTERN(int) git_status_file(unsigned int *status_flags, git_repository *repo, const char *path);

/** @} */
GIT_END_DECL
#endif
