/*
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 * - Neither the name of the Git Development Community nor the
 *   names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef INCLUDE_git_revwalk_h__
#define INCLUDE_git_revwalk_h__

#include "git_common.h"
#include "git_odb.h"
#include "git_commit.h"

/**
 * @file git_revwalk.h
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
GIT_EXTERN(git_revp*) git_revp_alloc(git_odb *db);

/**
 * Reset the traversal machinary for reuse.
 * @param pool traversal handle to reset.
 */
GIT_EXTERN(void) git_revp_reset(git_revp *pool);

/**
 * Mark an object to start traversal from.
 * @param pool the pool being used for the traversal.
 * @param commit the commit the commit to start from.
 */
GIT_EXTERN(void) git_revp_pushc(git_revp *pool, git_commit *commit);

/**
 * Mark a commit (and its ancestors) uninteresting for the output.
 * @param pool the pool being used for the traversal.
 * @param commit the commit the commit to start from.
 */
GIT_EXTERN(void) git_revp_hidec(git_revp *pool, git_commit *commit);

/**
 * Get the next commit from the revision traversal.
 * @param pool the pool to pop the commit from.
 * @return next commit; NULL if there is no more output.
 */
GIT_EXTERN(git_commit*) git_revp_nextc(git_revp *pool);

/**
 * Free a revwalk previously allocated.
 * @param pool traversal handle to close.  If NULL nothing occurs.
 */
GIT_EXTERN(void) git_revp_free(git_revp *pool);

/** @} */
GIT_END_DECL
#endif
