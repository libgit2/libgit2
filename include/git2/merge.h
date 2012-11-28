/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_merge_h__
#define INCLUDE_git_merge_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/merge.h
 * @brief Git merge-base routines
 * @defgroup git_revwalk Git merge-base routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Find a merge base between two commits
 *
 * @param out the OID of a merge base between 'one' and 'two'
 * @param repo the repository where the commits exist
 * @param one one of the commits
 * @param two the other commit
 * @return Zero on success; GIT_ENOTFOUND or -1 on failure.
 */
GIT_EXTERN(int) git_merge_base(
	git_oid *out,
	git_repository *repo,
	const git_oid *one,
	const git_oid *two);

/**
 * Find a merge base given a list of commits
 *
 * @param out the OID of a merge base considering all the commits
 * @param repo the repository where the commits exist
 * @param input_array oids of the commits
 * @param length The number of commits in the provided `input_array`
 * @return Zero on success; GIT_ENOTFOUND or -1 on failure.
 */
GIT_EXTERN(int) git_merge_base_many(
	git_oid *out,
	git_repository *repo,
	const git_oid input_array[],
	size_t length);

/**
 * Count the number of unique commits between two commit objects
 *
 * @param ahead number of commits, starting at `one`, unique from commits in `two` 
 * @param behind number of commits, starting at `two`, unique from commits in `one`
 * @param repo the repository where the commits exist
 * @param one one of the commits
 * @param two the other commit
 */
GIT_EXTERN(int) git_count_ahead_behind(int *ahead, int *behind, git_repository *repo, git_oid *one, git_oid *two);

/** @} */
GIT_END_DECL
#endif
