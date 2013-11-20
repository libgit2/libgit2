/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_merge_h__
#define INCLUDE_git_merge_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "checkout.h"
#include "index.h"

/**
 * @file git2/merge.h
 * @brief Git merge routines
 * @defgroup git_merge Git merge routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Flags for `git_merge_tree` options.  A combination of these flags can be
 * passed in via the `flags` value in the `git_merge_tree_opts`.
 */
typedef enum {
	/** Detect renames */
	GIT_MERGE_TREE_FIND_RENAMES = (1 << 0),
} git_merge_tree_flag_t;

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
	git_merge_tree_flag_t flags;

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
 * Option flags for `git_merge`.
 *
 * GIT_MERGE_NO_FASTFORWARD - Do not fast-forward.
 */
typedef enum {
	GIT_MERGE_NO_FASTFORWARD      = 1,
	GIT_MERGE_FASTFORWARD_ONLY    = 2,
} git_merge_flags_t;

typedef struct {
	unsigned int version;

	git_merge_flags_t merge_flags;
	git_merge_tree_opts merge_tree_opts;

	git_checkout_opts checkout_opts;
} git_merge_opts;

#define GIT_MERGE_OPTS_VERSION 1
#define GIT_MERGE_OPTS_INIT {GIT_MERGE_OPTS_VERSION, 0, GIT_MERGE_TREE_OPTS_INIT, GIT_CHECKOUT_OPTS_INIT}


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
 * @param length The number of commits in the provided `input_array`
 * @param input_array oids of the commits
 * @return Zero on success; GIT_ENOTFOUND or -1 on failure.
 */
GIT_EXTERN(int) git_merge_base_many(
	git_oid *out,
	git_repository *repo,
	size_t length,
	const git_oid input_array[]);

/**
 * Creates a `git_merge_head` from the given reference
 *
 * @param out pointer to store the git_merge_head result in
 * @param repo repository that contains the given reference
 * @param ref reference to use as a merge input
 * @return zero on success, -1 on failure.
 */
GIT_EXTERN(int) git_merge_head_from_ref(
	git_merge_head **out,
	git_repository *repo,
	git_reference *ref);

/**
 * Creates a `git_merge_head` from the given fetch head data
 *
 * @param out pointer to store the git_merge_head result in
 * @param repo repository that contains the given commit
 * @param branch_name name of the (remote) branch
 * @param remote_url url of the remote
 * @param oid the commit object id to use as a merge input
 * @return zero on success, -1 on failure.
 */
GIT_EXTERN(int) git_merge_head_from_fetchhead(
	git_merge_head **out,
	git_repository *repo,
	const char *branch_name,
	const char *remote_url,
	const git_oid *oid);

/**
 * Creates a `git_merge_head` from the given commit id
 *
 * @param out pointer to store the git_merge_head result in
 * @param repo repository that contains the given commit
 * @param oid the commit object id to use as a merge input
 * @return zero on success, -1 on failure.
 */
GIT_EXTERN(int) git_merge_head_from_oid(
	git_merge_head **out,
	git_repository *repo,
	const git_oid *oid);

/**
 * Frees a `git_merge_head`
 *
 * @param head merge head to free
 */
GIT_EXTERN(void) git_merge_head_free(
	git_merge_head *head);

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

/**
 * Merges the given commits into HEAD, producing a new commit.
 *
 * @param out the results of the merge
 * @param repo the repository to merge
 * @param merge_heads the heads to merge into
 * @param merge_heads_len the number of heads to merge
 * @param flags merge flags
 */
GIT_EXTERN(int) git_merge(
	git_merge_result **out,
	git_repository *repo,
	const git_merge_head **their_heads,
	size_t their_heads_len,
	const git_merge_opts *opts);

/**
 * Returns true if a merge is up-to-date (we were asked to merge the target
 * into itself.)
 */
GIT_EXTERN(int) git_merge_result_is_uptodate(git_merge_result *merge_result);

/**
 * Returns true if a merge is eligible for fastforward
 */
GIT_EXTERN(int) git_merge_result_is_fastforward(git_merge_result *merge_result);

/**
 * Gets the fast-forward OID if the merge was a fastforward.
 *
 * @param out the OID of the fast-forward
 * @param merge_result the results of the merge
 */
GIT_EXTERN(int) git_merge_result_fastforward_oid(git_oid *out, git_merge_result *merge_result);

GIT_EXTERN(void) git_merge_result_free(git_merge_result *merge_result);

/** @} */
GIT_END_DECL
#endif
