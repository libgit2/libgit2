/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_branch_h__
#define INCLUDE_git_branch_h__

#include "common.h"
#include "oid.h"
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
 * The returned reference must be freed by the user.
 *
 * The branch name will be checked for validity.
 * See `git_tag_create()` for rules about valid names.
 *
 * @param out Pointer where to store the underlying reference.
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
 * @return 0, GIT_EINVALIDSPEC or an error code.
 * A proper reference is written in the refs/heads namespace
 * pointing to the provided target commit.
 */
GIT_EXTERN(int) git_branch_create(
		git_reference **out,
		git_repository *repo,
		const char *branch_name,
		const git_commit *target,
		int force);

/**
 * Delete an existing branch reference.
 *
 * If the branch is successfully deleted, the passed reference
 * object will be freed and invalidated.
 *
 * @param branch A valid reference representing a branch
 * @return 0 on success, or an error code.
 */
GIT_EXTERN(int) git_branch_delete(git_reference *branch);

/**
 * Loop over all the branches and issue a callback for each one.
 *
 * If the callback returns a non-zero value, this will stop looping.
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
 * @return 0 on success, GIT_EUSER on non-zero callback, or error code
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
 * Move/rename an existing local branch reference.
 *
 * The new branch name will be checked for validity.
 * See `git_tag_create()` for rules about valid names.
 *
 * @param branch Current underlying reference of the branch.
 *
 * @param new_branch_name Target name of the branch once the move
 * is performed; this name is validated for consistency.
 *
 * @param force Overwrite existing branch.
 *
 * @return 0 on success, GIT_EINVALIDSPEC or an error code.
 */
GIT_EXTERN(int) git_branch_move(
		git_reference *branch,
		const char *new_branch_name,
		int force);

/**
 * Lookup a branch by its name in a repository.
 *
 * The generated reference must be freed by the user.
 *
 * The branch name will be checked for validity.
 * See `git_tag_create()` for rules about valid names.
 *
 * @param out pointer to the looked-up branch reference
 *
 * @param repo the repository to look up the branch
 *
 * @param branch_name Name of the branch to be looked-up;
 * this name is validated for consistency.
 *
 * @param branch_type Type of the considered branch. This should
 * be valued with either GIT_BRANCH_LOCAL or GIT_BRANCH_REMOTE.
 *
 * @return 0 on success; GIT_ENOTFOUND when no matching branch
 * exists, GIT_EINVALIDSPEC, otherwise an error code.
 */
GIT_EXTERN(int) git_branch_lookup(
		git_reference **out,
		git_repository *repo,
		const char *branch_name,
		git_branch_t branch_type);

/**
 * Return the name of the given local or remote branch.
 *
 * The name of the branch matches the definition of the name
 * for git_branch_lookup. That is, if the returned name is given
 * to git_branch_lookup() then the reference is returned that
 * was given to this function.
 *
 * @param out where the pointer of branch name is stored;
 * this is valid as long as the ref is not freed.
 * @param ref the reference ideally pointing to a branch
 *
 * @return 0 on success; otherwise an error code (e.g., if the
 *  ref is no local or remote branch).
 */
GIT_EXTERN(int) git_branch_name(const char **out,
		git_reference *ref);

/**
 * Return the reference supporting the remote tracking branch,
 * given a local branch reference.
 *
 * @param out Pointer where to store the retrieved
 * reference.
 *
 * @param branch Current underlying reference of the branch.
 *
 * @return 0 on success; GIT_ENOTFOUND when no remote tracking
 * reference exists, otherwise an error code.
 */
GIT_EXTERN(int) git_branch_tracking(
		git_reference **out,
		git_reference *branch);

/**
 * Return the name of the reference supporting the remote tracking branch,
 * given the name of a local branch reference.
 *
 * @param tracking_branch_name_out The user-allocated buffer which will be
 *     filled with the name of the reference. Pass NULL if you just want to
 *     get the needed size of the name of the reference as the output value.
 *
 * @param buffer_size Size of the `out` buffer in bytes.
 *
 * @param repo the repository where the branches live
 *
 * @param canonical_branch_name name of the local branch.
 *
 * @return number of characters in the reference name
 *     including the trailing NUL byte; GIT_ENOTFOUND when no remote tracking
 *     reference exists, otherwise an error code.
 */
GIT_EXTERN(int) git_branch_tracking_name(
	char *tracking_branch_name_out,
	size_t buffer_size,
	git_repository *repo,
	const char *canonical_branch_name);

/**
 * Determine if the current local branch is pointed at by HEAD.
 *
 * @param branch Current underlying reference of the branch.
 *
 * @return 1 if HEAD points at the branch, 0 if it isn't,
 * error code otherwise.
 */
GIT_EXTERN(int) git_branch_is_head(
		git_reference *branch);

/**
 * Return the name of remote that the remote tracking branch belongs to.
 *
 * @param remote_name_out The user-allocated buffer which will be
 *     filled with the name of the remote. Pass NULL if you just want to
 *     get the needed size of the name of the remote as the output value.
 *
 * @param buffer_size Size of the `out` buffer in bytes.
 *
 * @param repo The repository where the branch lives.
 *
 * @param canonical_branch_name name of the remote tracking branch.
 *
 * @return Number of characters in the reference name
 *     including the trailing NUL byte; GIT_ENOTFOUND
 *     when no remote matching remote was gound,
 *     GIT_EAMBIGUOUS when the branch maps to several remotes,
 *     otherwise an error code.
 */
GIT_EXTERN(int) git_branch_remote_name(
	char *remote_name_out,
	size_t buffer_size,
	git_repository *repo,
	const char *canonical_branch_name);

/** @} */
GIT_END_DECL
#endif
