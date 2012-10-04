/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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

enum {
	GIT_STASH_DEFAULT = 0,

	/* All changes already added to the index
	 * are left intact in the working directory
	 */
	GIT_STASH_KEEP_INDEX = (1 << 0),

	/* All untracked files are also stashed and then
	 * cleaned up from the working directory
	 */
	GIT_STASH_INCLUDE_UNTRACKED = (1 << 1),

	/* All ignored files are also stashed and then
	 * cleaned up from the working directory
	 */
	GIT_STASH_INCLUDE_IGNORED = (1 << 2),
};

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
 * @param flags Flags to control the stashing process.
 *
 * @return 0 on success, GIT_ENOTFOUND where there's nothing to stash,
 * or error code.
 */

GIT_EXTERN(int) git_stash_save(
	git_oid *out,
	git_repository *repo,
	git_signature *stasher,
	const char *message,
	uint32_t flags);

/** @} */
GIT_END_DECL
#endif
