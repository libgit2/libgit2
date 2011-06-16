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
#include "oid.h"

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
 * This revision walker uses a custom memory pool and an internal
 * commit cache, so it is relatively expensive to allocate.
 *
 * For maximum performance, this revision walker should be
 * reused for different walks.
 *
 * This revision walker is *not* thread safe: it may only be
 * used to walk a repository on a single thread; however,
 * it is possible to have several revision walkers in
 * several different threads walking the same repository.
 *
 * @param walker pointer to the new revision walker
 * @param repo the repo to walk through
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_revwalk_new(git_revwalk **walker, git_repository *repo);

/**
 * Reset the revision walker for reuse.
 *
 * This will clear all the pushed and hidden commits, and
 * leave the walker in a blank state (just like at
 * creation) ready to receive new commit pushes and
 * start a new walk.
 *
 * The revision walk is automatically reset when a walk
 * is over.
 *
 * @param walker handle to reset.
 */
GIT_EXTERN(void) git_revwalk_reset(git_revwalk *walker);

/**
 * Mark a commit to start traversal from.
 *
 * The given OID must belong to a commit on the walked
 * repository.
 *
 * The given commit will be used as one of the roots
 * when starting the revision walk. At least one commit
 * must be pushed the repository before a walk can
 * be started.
 *
 * @param walk the walker being used for the traversal.
 * @param oid the oid of the commit to start from.
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_revwalk_push(git_revwalk *walk, const git_oid *oid);


/**
 * Mark a commit (and its ancestors) uninteresting for the output.
 *
 * The given OID must belong to a commit on the walked
 * repository.
 *
 * The resolved commit and all its parents will be hidden from the
 * output on the revision walk.
 *
 * @param walk the walker being used for the traversal.
 * @param oid the oid of commit that will be ignored during the traversal
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_revwalk_hide(git_revwalk *walk, const git_oid *oid);

/**
 * Get the next commit from the revision walk.
 *
 * The initial call to this method is *not* blocking when
 * iterating through a repo with a time-sorting mode.
 *
 * Iterating with Topological or inverted modes makes the initial
 * call blocking to preprocess the commit list, but this block should be
 * mostly unnoticeable on most repositories (topological preprocessing
 * times at 0.3s on the git.git repo).
 *
 * The revision walker is reset when the walk is over.
 *
 * @param oid Pointer where to store the oid of the next commit
 * @param walk the walker to pop the commit from.
 * @return GIT_SUCCESS if the next commit was found;
 *	GIT_EREVWALKOVER if there are no commits left to iterate
 */
GIT_EXTERN(int) git_revwalk_next(git_oid *oid, git_revwalk *walk);

/**
 * Change the sorting mode when iterating through the
 * repository's contents.
 *
 * Changing the sorting mode resets the walker.
 *
 * @param walk the walker being used for the traversal.
 * @param sort_mode combination of GIT_SORT_XXX flags
 */
GIT_EXTERN(void) git_revwalk_sorting(git_revwalk *walk, unsigned int sort_mode);

/**
 * Free a revision walker previously allocated.
 *
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
