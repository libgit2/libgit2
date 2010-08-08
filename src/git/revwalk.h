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
 * Sort the repository contents in no particular ordering;
 * this sorting is arbritary, implementation-specific
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

typedef struct git_revwalk git_revwalk;

/**
 * Allocate a new revision walker to iterate through a repo.
 *
 * @param repo the repo to walk through
 * @return the new walker handle; NULL if memory is exhausted.
 */
GIT_EXTERN(git_revwalk *) git_revwalk_alloc(git_repository *repo);

/**
 * Reset the walking machinary for reuse.
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
GIT_EXTERN(void) git_revwalk_sorting(git_revwalk *walk, unsigned int sort_mode);

/**
 * Free a revwalk previously allocated.
 * @param walk traversal handle to close.  If NULL nothing occurs.
 */
GIT_EXTERN(void) git_revwalk_free(git_revwalk *walk);

/** @} */
GIT_END_DECL
#endif
