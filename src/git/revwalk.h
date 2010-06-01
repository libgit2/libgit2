#ifndef INCLUDE_git_revwalk_h__
#define INCLUDE_git_revwalk_h__

#include "common.h"
#include "odb.h"
#include "commit.h"

/**
 * @file git/revwalk.h
 * @brief Git revision traversal routines
 * @defgroup git_revwalk Git revision traversal routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Sort the revpool contents in no particular ordering;
 * this sorting is arbritary, implementation-specific
 * and subject to change at any time.
 * This is the default sorting for new revision pools.
 */
#define GIT_RPSORT_NONE         (0)

/**
 * Sort the revpool contents in topological order
 * (parents before children); this sorting mode
 * can be combined with time sorting.
 */
#define GIT_RPSORT_TOPOLOGICAL  (1 << 0)

/**
 * Sort the revpool contents by commit time;
 * this sorting mode can be combined with
 * topological sorting.
 */
#define GIT_RPSORT_TIME         (1 << 1)

/**
 * Iterate through the revpool contents in reverse
 * order; this sorting mode can be combined with
 * any of the above.
 */
#define GIT_RPSORT_REVERSE      (1 << 2)

/**
 * Allocate a new revision traversal pool.
 *
 * The configuration is copied during allocation.  Changes
 * to the configuration after allocation do not affect the pool
 * returned by this function.  Callers may safely free the
 * passed configuration after the function completes.
 *
 * @param db the database objects are read from.
 * @return the new traversal handle; NULL if memory is exhausted.
 */
GIT_EXTERN(git_revpool *) gitrp_alloc(git_odb *db);

/**
 * Reset the traversal machinary for reuse.
 * @param pool traversal handle to reset.
 */
GIT_EXTERN(void) gitrp_reset(git_revpool *pool);

/**
 * Mark an object to start traversal from.
 * @param pool the pool being used for the traversal.
 * @param commit the commit to start from.
 */
GIT_EXTERN(int) gitrp_push(git_revpool *pool, git_commit *commit);

/**
 * Mark a commit (and its ancestors) uninteresting for the output.
 * @param pool the pool being used for the traversal.
 * @param commit the commit that will be ignored during the traversal
 */
GIT_EXTERN(int) gitrp_hide(git_revpool *pool, git_commit *commit);

/**
 * Get the next commit from the revision traversal.
 * @param pool the pool to pop the commit from.
 * @return next commit; NULL if there is no more output.
 */
GIT_EXTERN(git_commit *) gitrp_next(git_revpool *pool);

/**
 * Change the sorting mode when iterating through the
 * revision pool's contents.
 * @param pool the pool being used for the traversal.
 * @param sort_mode combination of GIT_RPSORT_XXX flags
 */
GIT_EXTERN(void) gitrp_sorting(git_revpool *pool, unsigned int sort_mode);

/**
 * Free a revwalk previously allocated.
 * @param pool traversal handle to close.  If NULL nothing occurs.
 */
GIT_EXTERN(void) gitrp_free(git_revpool *pool);

/** @} */
GIT_END_DECL
#endif
