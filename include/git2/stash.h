/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_stash_h__
#define INCLUDE_git_stash_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/stash.h
 * @brief Git stash management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Stash flags
 */
typedef enum {
	/**
	 * No option, default
	 */
	GIT_STASH_DEFAULT = 0,

	/**
	 * All changes already added to the index are left intact in
	 * the working directory
	 */
	GIT_STASH_KEEP_INDEX = (1 << 0),

	/**
	 * All untracked files are also stashed and then cleaned up
	 * from the working directory
	 */
	GIT_STASH_INCLUDE_UNTRACKED = (1 << 1),

	/**
	 * All ignored files are also stashed and then cleaned up from
	 * the working directory
	 */
	GIT_STASH_INCLUDE_IGNORED = (1 << 2),
} git_stash_flags;

/**
 * Save the local modifications to a new stash.
 *
 * @param out Object id of the commit containing the stashed state.
 * This commit is also the target of the direct reference refs/stash.
 *
 * @param repo The owning repository.
 *
 * @param stasher The identity of the person performing the stashing.
 *
 * @param message Optional description along with the stashed state.
 *
 * @param flags Flags to control the stashing process. (see GIT_STASH_* above)
 *
 * @return 0 on success, GIT_ENOTFOUND where there's nothing to stash,
 * or error code.
 */
GIT_EXTERN(int) git_stash_save(
	git_oid *out,
	git_repository *repo,
	const git_signature *stasher,
	const char *message,
	unsigned int flags);

typedef enum {
	GIT_APPLY_DEFAULT = 0,

	/* Try to reinstate not only the working tree's changes,
	 * but also the index's ones.
	 */
	GIT_APPLY_REINSTATE_INDEX = (1 << 0),
} git_apply_flags;

/**
 * Apply a single stashed state from the stash list.
 *
 * If any untracked or ignored file saved in the stash already exist in the
 * workdir, the function will return GIT_EEXISTS and both the workdir and index
 * will be left untouched.
 *
 * If local changes in the workdir would be overwritten when applying
 * modifications saved in the stash, the function will return GIT_EMERGECONFLICT
 * and the index will be left untouched. The workdir files will be left
 * unmodified as well but restored untracked or ignored files that were saved
 * in the stash will be left around in the workdir.
 *
 * If passing the GIT_APPLY_REINSTATE_INDEX flag and there would be conflicts
 * when reinstating the index, the function will return GIT_EUNMERGED and both
 * the workdir and index will be left untouched.
 *
 * @param repo The owning repository.
 *
 * @param index The position within the stash list. 0 points to the
 * most recent stashed state.
 *
 * @param flags Flags to control the applying process. (see GIT_APPLY_* above)
 *
 * @return 0 on success, GIT_ENOTFOUND if there's no stashed state for the given
 * index, or error code. (see details above)
 */
GIT_EXTERN(int) git_stash_apply(
	git_repository *repo,
	size_t index,
	unsigned int flags);

/**
 * This is a callback function you can provide to iterate over all the
 * stashed states that will be invoked per entry.
 *
 * @param index The position within the stash list. 0 points to the
 *              most recent stashed state.
 * @param message The stash message.
 * @param stash_id The commit oid of the stashed state.
 * @param payload Extra parameter to callback function.
 * @return 0 to continue iterating or non-zero to stop.
 */
typedef int (*git_stash_cb)(
	size_t index,
	const char* message,
	const git_oid *stash_id,
	void *payload);

/**
 * Loop over all the stashed states and issue a callback for each one.
 *
 * If the callback returns a non-zero value, this will stop looping.
 *
 * @param repo Repository where to find the stash.
 *
 * @param callback Callback to invoke per found stashed state. The most
 *                 recent stash state will be enumerated first.
 *
 * @param payload Extra parameter to callback function.
 *
 * @return 0 on success, non-zero callback return value, or error code.
 */
GIT_EXTERN(int) git_stash_foreach(
	git_repository *repo,
	git_stash_cb callback,
	void *payload);

/**
 * Remove a single stashed state from the stash list.
 *
 * @param repo The owning repository.
 *
 * @param index The position within the stash list. 0 points to the
 * most recent stashed state.
 *
 * @return 0 on success, GIT_ENOTFOUND if there's no stashed state for the given
 * index, or error code.
 */
GIT_EXTERN(int) git_stash_drop(
	git_repository *repo,
	size_t index);

/**
 * Apply a single stashed state from the stash list and remove it from the list
 * if successful.
 *
 * @param repo The owning repository.
 *
 * @param index The position within the stash list. 0 points to the
 * most recent stashed state.
 *
 * @param flags Flags to control the applying process. (see GIT_APPLY_* above)
 *
 * @return 0 on success, GIT_ENOTFOUND if there's no stashed state for the given
 * index, or error code. (see git_stash_apply() above for details)
*/
GIT_EXTERN(int) git_stash_pop(
	git_repository *repo,
	size_t index,
	unsigned int flags);

/** @} */
GIT_END_DECL
#endif
