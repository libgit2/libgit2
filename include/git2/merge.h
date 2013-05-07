/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_merge_h__
#define INCLUDE_git_merge_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"
#include "git2/checkout.h"
#include "git2/index.h"

/**
 * @file git2/merge.h
 * @brief Git merge routines
 * @defgroup git_merge Git merge routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Flags for tree_many diff options.  A combination of these flags can be
 * passed in via the `flags` value in the `git_diff_tree_many_options`.
 */
typedef enum {
	/** Detect renames */
	GIT_MERGE_TREE_FIND_RENAMES = (1 << 0),
} git_merge_tree_flags;

/**
 * Automerge options for `git_merge_trees_opts`.
 */ 
typedef enum {
	GIT_MERGE_AUTOMERGE_NORMAL = 0,
	GIT_MERGE_AUTOMERGE_NONE = 1,
	GIT_MERGE_AUTOMERGE_FAVOR_OURS = 2,
	GIT_MERGE_AUTOMERGE_FAVOR_THEIRS = 3,
} git_merge_automerge_flags;


typedef struct {
	unsigned int version;
	git_merge_tree_flags flags;
	
	/** Similarity to consider a file renamed (default 50) */
	unsigned int rename_threshold;
	
	/** Maximum similarity sources to examine (overrides the
	 * `merge.renameLimit` config) (default 200)
	 */
	unsigned int target_limit;

    /** Pluggable similarity metric; pass NULL to use internal metric */
	git_diff_similarity_metric *metric;
	
	/** Flags for automerging content. */
	git_merge_automerge_flags automerge_flags;
} git_merge_tree_opts;

#define GIT_MERGE_TREE_OPTS_VERSION 1
#define GIT_MERGE_TREE_OPTS_INIT {GIT_MERGE_TREE_OPTS_VERSION}


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
 * Merge two trees, producing a `git_index` that reflects the result of
 * the merge.
 *
 * The returned index must be freed explicitly with `git_index_free`.
 *
 * @param out pointer to store the index result in
 * @param repo repository that contains the given trees
 * @param ancestor_tree the common ancestor between the trees (or null if none)
 * @param our_tree the tree that reflects the destination tree
 * @param their_tree the tree to merge in to `our_tree`
 * @param opts the merge tree options (or null for defaults)
 * @return zero on success, -1 on failure.
 */
GIT_EXTERN(int) git_merge_trees(
	git_index **out,
	git_repository *repo,
	const git_tree *ancestor_tree,
	const git_tree *our_tree,
	const git_tree *their_tree,
	const git_merge_tree_opts *opts);

/** @} */
GIT_END_DECL
#endif
