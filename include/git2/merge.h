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
	/**
	 * Detect renames that occur between the common ancestor and the "ours"
	 * side or the common ancestor and the "theirs" side.  This will enable
	 * the ability to merge between a modified and renamed file.
	 */
	GIT_MERGE_TREE_FIND_RENAMES = (1 << 0),
} git_merge_tree_flag_t;

/**
 * Merge file favor options for `git_merge_trees_opts` instruct the file-level
 * merging functionality how to deal with conflicting regions of the files.
 */
typedef enum {
	/**
	 * When a region of a file is changed in both branches, a conflict
	 * will be recorded in the index so that `git_checkout` can produce
	 * a merge file with conflict markers in the working directory.
	 * This is the default.
	 */
	GIT_MERGE_FILE_FAVOR_NORMAL = 0,

	/**
	 * When a region of a file is changed in both branches, the file
	 * created in the index will contain the "ours" side of any conflicting
	 * region.  The index will not record a conflict.
	 */
	GIT_MERGE_FILE_FAVOR_OURS = 1,

	/**
	 * When a region of a file is changed in both branches, the file
	 * created in the index will contain the "theirs" side of any conflicting
	 * region.  The index will not record a conflict.
	 */
	GIT_MERGE_FILE_FAVOR_THEIRS = 2,

	/**
	 * When a region of a file is changed in both branches, the file
	 * created in the index will contain each unique line from each side,
	 * which has the result of combining both files.  The index will not
	 * record a conflict.
	 */
	GIT_MERGE_FILE_FAVOR_UNION = 3,
} git_merge_file_favor_t;


typedef struct {
	unsigned int version;
	git_merge_tree_flag_t flags;

	/**
	 * Similarity to consider a file renamed (default 50).  If
	 * `GIT_MERGE_TREE_FIND_RENAMES` is enabled, added files will be compared
	 * with deleted files to determine their similarity.  Files that are
	 * more similar than the rename threshold (percentage-wise) will be
	 * treated as a rename.
	 */
	unsigned int rename_threshold;

	/**
	 * Maximum similarity sources to examine for renames (default 200).
	 * If the number of rename candidates (add / delete pairs) is greater
	 * than this value, inexact rename detection is aborted.
	 *
	 * This setting overrides the `merge.renameLimit` configuration value.
	 */
	unsigned int target_limit;

	/** Pluggable similarity metric; pass NULL to use internal metric */
	git_diff_similarity_metric *metric;

	/** Flags for handling conflicting content. */
	git_merge_file_favor_t file_favor;
} git_merge_tree_opts;

#define GIT_MERGE_TREE_OPTS_VERSION 1
#define GIT_MERGE_TREE_OPTS_INIT {GIT_MERGE_TREE_OPTS_VERSION}

/**
 * Initializes a `git_merge_tree_opts` with default values. Equivalent to
 * creating an instance with GIT_MERGE_TREE_OPTS_INIT.
 *
 * @param opts the `git_merge_tree_opts` instance to initialize.
 * @param version the version of the struct; you should pass
 *        `GIT_MERGE_TREE_OPTS_VERSION` here.
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_merge_tree_init_opts(
	git_merge_tree_opts* opts,
	int version);

/**
 * Option flags for `git_merge`.
 */
typedef enum {
	/**
	 * The default behavior is to allow fast-forwards, returning
	 * immediately with the commit ID to fast-forward to.
	 */
	GIT_MERGE_DEFAULT = 0,

	/**
	 * Do not fast-forward; perform a merge and prepare a merge result even
	 * if the inputs are eligible for fast-forwarding.
	 */
	GIT_MERGE_NO_FASTFORWARD = 1,

	/**
	 * Ensure that the inputs are eligible for fast-forwarding, error if
	 * a merge needs to be performed.
	 */
	GIT_MERGE_FASTFORWARD_ONLY = 2,
} git_merge_flags_t;

typedef struct {
	unsigned int version;

	/** Options for handling the commit-level merge. */
	git_merge_flags_t merge_flags;

	/** Options for handling the merges of individual files. */
	git_merge_tree_opts merge_tree_opts;

	/** Options for writing the merge result to the working directory. */
	git_checkout_options checkout_opts;
} git_merge_opts;

#define GIT_MERGE_OPTS_VERSION 1
#define GIT_MERGE_OPTS_INIT {GIT_MERGE_OPTS_VERSION, 0, GIT_MERGE_TREE_OPTS_INIT, GIT_CHECKOUT_OPTIONS_INIT}

/**
 * Initializes a `git_merge_opts` with default values. Equivalent to creating
 * an instance with GIT_MERGE_OPTS_INIT.
 *
 * @param opts the `git_merge_opts` instance to initialize.
 * @param version the version of the struct; you should pass
 *        `GIT_MERGE_OPTS_VERSION` here.
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_merge_init_opts(
	git_merge_opts* opts,
	int version);

/**
 * Find a merge base between two commits
 *
 * @param out the OID of a merge base between 'one' and 'two'
 * @param repo the repository where the commits exist
 * @param one one of the commits
 * @param two the other commit
 * @return 0 on success, GIT_ENOTFOUND if not found or error code
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
 * @return 0 on success, GIT_ENOTFOUND if not found or error code
 */
GIT_EXTERN(int) git_merge_base_many(
	git_oid *out,
	git_repository *repo,
	size_t length,
	const git_oid input_array[]);

/**
 * Creates a `git_merge_head` from the given reference.  The resulting
 * git_merge_head must be freed with `git_merge_head_free`.
 *
 * @param out pointer to store the git_merge_head result in
 * @param repo repository that contains the given reference
 * @param ref reference to use as a merge input
 * @return 0 on success or error code
 */
GIT_EXTERN(int) git_merge_head_from_ref(
	git_merge_head **out,
	git_repository *repo,
	git_reference *ref);

/**
 * Creates a `git_merge_head` from the given fetch head data.  The resulting
 * git_merge_head must be freed with `git_merge_head_free`.
 *
 * @param out pointer to store the git_merge_head result in
 * @param repo repository that contains the given commit
 * @param branch_name name of the (remote) branch
 * @param remote_url url of the remote
 * @param oid the commit object id to use as a merge input
 * @return 0 on success or error code
 */
GIT_EXTERN(int) git_merge_head_from_fetchhead(
	git_merge_head **out,
	git_repository *repo,
	const char *branch_name,
	const char *remote_url,
	const git_oid *oid);

/**
 * Creates a `git_merge_head` from the given commit id.  The resulting
 * git_merge_head must be freed with `git_merge_head_free`.
 *
 * @param out pointer to store the git_merge_head result in
 * @param repo repository that contains the given commit
 * @param id the commit object id to use as a merge input
 * @return 0 on success or error code
 */
GIT_EXTERN(int) git_merge_head_from_id(
	git_merge_head **out,
	git_repository *repo,
	const git_oid *id);

/**
 * Frees a `git_merge_head`.
 *
 * @param head merge head to free
 */
GIT_EXTERN(void) git_merge_head_free(
	git_merge_head *head);

/**
 * Merge two trees, producing a `git_index` that reflects the result of
 * the merge.  The index may be written as-is to the working directory
 * or checked out.  If the index is to be converted to a tree, the caller
 * should resolve any conflicts that arose as part of the merge.
 *
 * The returned index must be freed explicitly with `git_index_free`.
 *
 * @param out pointer to store the index result in
 * @param repo repository that contains the given trees
 * @param ancestor_tree the common ancestor between the trees (or null if none)
 * @param our_tree the tree that reflects the destination tree
 * @param their_tree the tree to merge in to `our_tree`
 * @param opts the merge tree options (or null for defaults)
 * @return 0 on success or error code
 */
GIT_EXTERN(int) git_merge_trees(
	git_index **out,
	git_repository *repo,
	const git_tree *ancestor_tree,
	const git_tree *our_tree,
	const git_tree *their_tree,
	const git_merge_tree_opts *opts);

/**
 * Merge two commits, producing a `git_index` that reflects the result of
 * the merge.  The index may be written as-is to the working directory
 * or checked out.  If the index is to be converted to a tree, the caller
 * should resolve any conflicts that arose as part of the merge.
 *
 * The returned index must be freed explicitly with `git_index_free`.
 *
 * @param out pointer to store the index result in
 * @param repo repository that contains the given trees
 * @param our_commit the commit that reflects the destination tree
 * @param their_commit the commit to merge in to `our_commit`
 * @param opts the merge tree options (or null for defaults)
 * @return 0 on success or error code
 */
GIT_EXTERN(int) git_merge_commits(
	git_index **out,
	git_repository *repo,
	const git_commit *our_commit,
	const git_commit *their_commit,
	const git_merge_tree_opts *opts);

/**
 * Merges the given commit(s) into HEAD and either returns immediately
 * if there was no merge to perform (the specified commits have already
 * been merged or would produce a fast-forward) or performs the merge
 * and writes the results into the working directory.
 *
 * Callers should inspect the `git_merge_result`:
 *
 * If `git_merge_result_is_uptodate` is true, there is no work to perform.
 *
 * If `git_merge_result_is_fastforward` is true, the caller should update
 * any necessary references to the commit ID returned by
 * `git_merge_result_fastforward_id` and check that out in order to complete
 * the fast-forward.
 * 
 * Otherwise, callers should inspect the resulting index, resolve any
 * conflicts and prepare a commit.
 *
 * The resultant `git_merge_result` should be free with
 * `git_merge_result_free`.
 *
 * @param out the results of the merge
 * @param repo the repository to merge
 * @param merge_heads the heads to merge into
 * @param merge_heads_len the number of heads to merge
 * @param opts merge options
 * @return 0 on success or error code
 */
GIT_EXTERN(int) git_merge(
	git_merge_result **out,
	git_repository *repo,
	const git_merge_head **their_heads,
	size_t their_heads_len,
	const git_merge_opts *opts);

/**
 * Returns true if a merge is "up-to-date", meaning that the commit(s) 
 * that were provided to `git_merge` are already included in `HEAD`
 * and there is no work to do.
 *
 * @return true if the merge is up-to-date, false otherwise
 */
GIT_EXTERN(int) git_merge_result_is_uptodate(git_merge_result *merge_result);

/**
 * Returns true if a merge is eligible to be "fast-forwarded", meaning that
 * the commit that was provided to `git_merge` need not be merged, it can
 * simply be checked out, because the current `HEAD` is the merge base of
 * itself and the given commit.  To perform the fast-forward, the caller
 * should check out the results of `git_merge_result_fastforward_id`.
 * 
 * This will never be true if `GIT_MERGE_NO_FASTFORWARD` is supplied as
 * a merge option.
 *
 * @return true if the merge is fast-forwardable, false otherwise
 */
GIT_EXTERN(int) git_merge_result_is_fastforward(git_merge_result *merge_result);

/**
 * Gets the fast-forward OID if the merge was a fastforward.
 *
 * @param out pointer to populate with the OID of the fast-forward
 * @param merge_result the results of the merge
 * @return 0 on success or error code
 */
GIT_EXTERN(int) git_merge_result_fastforward_id(git_oid *out, git_merge_result *merge_result);

/**
 * Frees a `git_merge_result`.
 *
 * @param result merge result to free
 */
GIT_EXTERN(void) git_merge_result_free(git_merge_result *merge_result);

/** @} */
GIT_END_DECL
#endif
