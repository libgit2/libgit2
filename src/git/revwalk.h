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
GIT_EXTERN(git_revpool*) gitrp_alloc(git_odb *db);

/**
 * Reset the traversal machinary for reuse.
 * @param pool traversal handle to reset.
 */
GIT_EXTERN(void) gitrp_reset(git_revpool *pool);

/**
 * Mark an object to start traversal from.
 * @param pool the pool being used for the traversal.
 * @param commit the commit the commit to start from.
 */
GIT_EXTERN(void) gitrp_push(git_revpool *pool, git_commit *commit);

/**
 * Mark a commit (and its ancestors) uninteresting for the output.
 * @param pool the pool being used for the traversal.
 * @param commit the commit the commit to start from.
 */
GIT_EXTERN(void) gitrp_hide(git_revpool *pool, git_commit *commit);

/**
 * Get the next commit from the revision traversal.
 * @param pool the pool to pop the commit from.
 * @return next commit; NULL if there is no more output.
 */
GIT_EXTERN(git_commit*) gitrp_next(git_revpool *pool);

/**
 * Free a revwalk previously allocated.
 * @param pool traversal handle to close.  If NULL nothing occurs.
 */
GIT_EXTERN(void) gitrp_free(git_revpool *pool);

/** @} */
GIT_END_DECL
#endif
