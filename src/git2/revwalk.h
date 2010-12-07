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
#ifndef INCLUDE_git_revwalk_h__
#define INCLUDE_git_revwalk_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/revwalk.h
 * @brief Git revision traversal routines
 * @defgroup git_revwalk Git revision traversal routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Sort the repository contents in no particular ordering;
 * this sorting is arbitrary, implementation-specific
 * and subject to change at any time.
 * This is the default sorting for new walkers.
 */
#define GIT_SORT_NONE         (0)

/**
 * Sort the repository contents in topological order
 * (parents before children); this sorting mode
 * can be combined with time sorting.
 */
#define GIT_SORT_TOPOLOGICAL  (1 << 0)

/**
 * Sort the repository contents by commit time;
 * this sorting mode can be combined with
 * topological sorting.
 */
#define GIT_SORT_TIME         (1 << 1)

/**
 * Iterate through the repository contents in reverse
 * order; this sorting mode can be combined with
 * any of the above.
 */
#define GIT_SORT_REVERSE      (1 << 2)

/**
 * Allocate a new revision walker to iterate through a repo.
 *
 * @param walker pointer to the new revision walker
 * @param repo the repo to walk through
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_revwalk_new(git_revwalk **walker, git_repository *repo);

/**
 * Reset the walking machinery for reuse.
 * @param walker handle to reset.
 */
GIT_EXTERN(void) git_revwalk_reset(git_revwalk *walker);

/**
 * Mark a commit to start traversal from.
 * The commit object must belong to the repo which is being walked through.
 *
 * @param walker the walker being used for the traversal.
 * @param commit the commit to start from.
 */
GIT_EXTERN(int) git_revwalk_push(git_revwalk *walk, git_commit *commit);

/**
 * Mark a commit (and its ancestors) uninteresting for the output.
 * @param walker the walker being used for the traversal.
 * @param commit the commit that will be ignored during the traversal
 */
GIT_EXTERN(int) git_revwalk_hide(git_revwalk *walk, git_commit *commit);

/**
 * Get the next commit from the revision traversal.
 * @param walk the walker to pop the commit from.
 * @return next commit; NULL if there is no more output.
 */
GIT_EXTERN(git_commit *) git_revwalk_next(git_revwalk *walk);

/**
 * Change the sorting mode when iterating through the
 * repository's contents.
 * Changing the sorting mode resets the walker.
 * @param walk the walker being used for the traversal.
 * @param sort_mode combination of GIT_RPSORT_XXX flags
 */
GIT_EXTERN(int) git_revwalk_sorting(git_revwalk *walk, unsigned int sort_mode);

/**
 * Free a revwalk previously allocated.
 * @param walk traversal handle to close.  If NULL nothing occurs.
 */
GIT_EXTERN(void) git_revwalk_free(git_revwalk *walk);

/**
 * Return the repository on which this walker
 * is operating.
 *
 * @param walk the revision walker
 * @return the repository being walked
 */
GIT_EXTERN(git_repository *) git_revwalk_repository(git_revwalk *walk);

/** @} */
GIT_END_DECL
#endif
