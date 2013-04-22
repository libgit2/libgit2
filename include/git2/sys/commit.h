/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_commit_h__
#define INCLUDE_sys_git_commit_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"

/**
 * @file git2/sys/commit.h
 * @brief Low-level Git commit creation
 * @defgroup git_backend Git custom backend APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Create new commit in the repository from a list of `git_oid` values
 *
 * See documentation for `git_commit_create()` for information about the
 * parameters, as the meaning is identical excepting that `tree` and
 * `parents` now take `git_oid`.  This is a dangerous API in that the
 * `parents` list of `git_oid`s in not checked for validity.
 */
GIT_EXTERN(int) git_commit_create_from_oids(
	git_oid *oid,
	git_repository *repo,
	const char *update_ref,
	const git_signature *author,
	const git_signature *committer,
	const char *message_encoding,
	const char *message,
	const git_oid *tree,
	int parent_count,
	const git_oid *parents[]);

/** @} */
GIT_END_DECL
#endif
