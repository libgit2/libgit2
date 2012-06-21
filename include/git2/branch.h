/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_branch_h__
#define INCLUDE_git_branch_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/branch.h
 * @brief Git branch parsing routines
 * @defgroup git_branch Git branch management
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Create a new branch pointing at a target commit
 *
 * A new direct reference will be created pointing to
 * this target commit. If `force` is true and a reference
 * already exists with the given name, it'll be replaced.
 *
 * @param oid_out Pointer where to store the OID of the target commit.
 *
 * @param repo Repository where to store the branch.
 *
 * @param branch_name Name for the branch; this name is
 * validated for consistency. It should also not conflict with
 * an already existing branch name.
 *
 * @param target Object to which this branch should point. This object
 * must belong to the given `repo` and can either be a git_commit or a
 * git_tag. When a git_tag is being passed, it should be dereferencable
 * to a git_commit which oid will be used as the target of the branch.
 *
 * @param force Overwrite existing branch.
 *
 * @return 0 or an error code.
 * A proper reference is written in the refs/heads namespace
 * pointing to the provided target commit.
 */
GIT_EXTERN(int) git_branch_create(
		git_oid *oid_out,
		git_repository *repo,
		const char *branch_name,
		const git_object *target,
		int force);

/**
 * Delete an existing branch reference.
 *
 * @param repo Repository where lives the branch.
 *
 * @param branch_name Name of the branch to be deleted;
 * this name is validated for consistency.
 *
 * @param branch_type Type of the considered branch. This should
 * be valued with either GIT_BRANCH_LOCAL or GIT_BRANCH_REMOTE.
 *
 * @return 0 on success, GIT_ENOTFOUND if the branch
 * doesn't exist or an error code.
 */
GIT_EXTERN(int) git_branch_delete(
		git_repository *repo,
		const char *branch_name,
		git_branch_t branch_type);

/**
 * Loop over all the branches and issue a callback for each one.
 *
 * @param repo Repository where to find the branches.
 *
 * @param list_flags Filtering flags for the branch
 * listing. Valid values are GIT_BRANCH_LOCAL, GIT_BRANCH_REMOTE
 * or a combination of the two.
 *
 * @param branch_cb Callback to invoke per found branch.
 *
 * @param payload Extra parameter to callback function.
 *
 * @return 0 or an error code.
 */
GIT_EXTERN(int) git_branch_foreach(
		git_repository *repo,
		unsigned int list_flags,
		int (*branch_cb)(
			const char *branch_name,
			git_branch_t branch_type,
			void *payload),
		void *payload
);

/**
 * Move/rename an existing branch reference.
 *
 * @param repo Repository where lives the branch.
 *
 * @param old_branch_name Current name of the branch to be moved;
 * this name is validated for consistency.
 *
 * @param new_branch_name Target name of the branch once the move
 * is performed; this name is validated for consistency.
 *
 * @param force Overwrite existing branch.
 *
 * @return 0 on success, GIT_ENOTFOUND if the branch
 * doesn't exist or an error code.
 */
GIT_EXTERN(int) git_branch_move(
		git_repository *repo,
		const char *old_branch_name,
		const char *new_branch_name,
		int force);

/** @} */
GIT_END_DECL
#endif
