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
