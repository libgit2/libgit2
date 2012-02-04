/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_diff_h__
#define INCLUDE_git_diff_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "tree.h"
#include "refs.h"

/**
 * @file git2/diff.h
 * @brief Git tree and file differencing routines.
 *
 * Calculating diffs is generally done in two phases: building a diff list
 * then traversing the diff list.  This makes is easier to share logic
 * across the various types of diffs (tree vs tree, workdir vs index, etc.),
 * and also allows you to insert optional diff list post-processing phases,
 * such as rename detected, in between the steps.  When you are done with a
 * diff list object, it must be freed.
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Structure describing options about how the diff should be executed.
 *
 * @todo Most of the parameters here are not actually supported at this time.
 */
typedef struct {
	int context_lines;
	int interhunk_lines;
	int ignore_whitespace;
	int force_text;			/**< generate text diffs even for binaries */
	git_strarray pathspec;
} git_diff_options;

/**
 * The diff list object that contains all individual file deltas.
 */
typedef struct git_diff_list git_diff_list;

/**
 * Description of changes to one file.
 *
 * When iterating over a diff list object, this will generally be passed to
 * most callback functions and you can use the contents to understand
 * exactly what has changed.
 *
 * Under some circumstances, not all fields will be filled in, but the code
 * generally tries to fill in as much as possible.  One example is that the
 * "binary" field will not actually look at file contents if you do not
 * pass in hunk and/or line callbacks to the diff foreach iteration function.
 * It will just use the git attributes for those files.
 */
typedef struct {
	git_status_t status;     /**< value from tree.h */
	unsigned int old_attr;
	unsigned int new_attr;
	git_oid      old_oid;
	git_oid      new_oid;
	git_blob     *old_blob;
	git_blob     *new_blob;
	const char   *path;
	const char   *new_path;  /**< NULL unless status is RENAMED or COPIED */
	int          similarity; /**< for RENAMED and COPIED, value from 0 to 100 */
	int          binary;     /**< files in diff are binary? */
} git_diff_delta;

/**
 * When iterating over a diff, callback that will be made per file.
 */
typedef int (*git_diff_file_fn)(
	void *cb_data,
	git_diff_delta *delta,
	float progress);

/**
 * Structure describing a hunk of a diff.
 */
typedef struct {
	int old_start;
	int old_lines;
	int new_start;
	int new_lines;
} git_diff_range;

/**
 * When iterating over a diff, callback that will be made per hunk.
 */
typedef int (*git_diff_hunk_fn)(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	const char *header,
	size_t header_len);

/**
 * Line origin constants.
 *
 * These values describe where a line came from and will be passed to
 * the git_diff_line_fn when iterating over a diff.  There are some
 * special origin contants at the end that are used for the text
 * output callbacks to demarcate lines that are actually part of
 * the file or hunk headers.
 */
enum {
	/* these values will be sent to `git_diff_line_fn` along with the line */
	GIT_DIFF_LINE_CONTEXT   = ' ',
	GIT_DIFF_LINE_ADDITION  = '+',
	GIT_DIFF_LINE_DELETION  = '-',
	GIT_DIFF_LINE_ADD_EOFNL = '\n', /**< LF was added at end of file */
	GIT_DIFF_LINE_DEL_EOFNL = '\0', /**< LF was removed at end of file */
	/* these values will only be sent to a `git_diff_output_fn` */
	GIT_DIFF_LINE_FILE_HDR  = 'F',
	GIT_DIFF_LINE_HUNK_HDR  = 'H',
	GIT_DIFF_LINE_BINARY    = 'B'
};

/**
 * When iterating over a diff, callback that will be made per text diff
 * line.
 */
typedef int (*git_diff_line_fn)(
	void *cb_data,
	git_diff_delta *delta,
	char line_origin, /**< GIT_DIFF_LINE_... value from above */
	const char *content,
	size_t content_len);

/**
 * When printing a diff, callback that will be made to output each line
 * of text.  This uses some extra GIT_DIFF_LINE_... constants for output
 * of lines of file and hunk headers.
 */
typedef int (*git_diff_output_fn)(
	void *cb_data,
	char line_origin, /**< GIT_DIFF_LINE_... value from above */
	const char *formatted_output);


/** @name Diff List Generator Functions
 *
 * These are the functions you would use to create (or destroy) a
 * git_diff_list from various objects in a repository.
 */
/**@{*/

/**
 * Compute a difference between two tree objects.
 */
GIT_EXTERN(int) git_diff_tree_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_tree *new,
	git_diff_list **diff);

/**
 * Compute a difference between a tree and the index.
 * @todo NOT IMPLEMENTED
 */
GIT_EXTERN(int) git_diff_index_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff);

/**
 * Compute a difference between the working directory and a tree.
 * @todo NOT IMPLEMENTED
 */
GIT_EXTERN(int) git_diff_workdir_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff);

/**
 * Compute a difference between the working directory and the index.
 * @todo NOT IMPLEMENTED
 */
GIT_EXTERN(int) git_diff_workdir_to_index(
	git_repository *repo,
	const git_diff_options *opts,
	git_diff_list **diff);

/**
 * Deallocate a diff list.
 */
GIT_EXTERN(void) git_diff_list_free(git_diff_list *diff);

/**@}*/


/** @name Diff List Processor Functions
 *
 * These are the functions you apply to a diff list to process it
 * or read it in some way.
 */
/**@{*/

/**
 * Iterate over a diff list issuing callbacks.
 *
 * If the hunk and/or line callbacks are not NULL, then this will calculate
 * text diffs for all files it thinks are not binary.  If those are both
 * NULL, then this will not bother with the text diffs, so it can be
 * efficient.
 */
GIT_EXTERN(int) git_diff_foreach(
	git_diff_list *diff,
	void *cb_data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_line_fn line_cb);

/**
 * Iterate over a diff generating text output like "git diff --name-status".
 */
GIT_EXTERN(int) git_diff_print_compact(
	git_diff_list *diff,
	void *cb_data,
	git_diff_output_fn print_cb);

/**
 * Iterate over a diff generating text output like "git diff".
 *
 * This is a super easy way to generate a patch from a diff.
 */
GIT_EXTERN(int) git_diff_print_patch(
	git_diff_list *diff,
	void *cb_data,
	git_diff_output_fn print_cb);

/**@}*/


/*
 * Misc
 */

/**
 * Directly run a text diff on two blobs.
 */
GIT_EXTERN(int) git_diff_blobs(
	git_repository *repo,
	git_blob *old,
	git_blob *new,
	git_diff_options *options,
	void *cb_data,
	git_diff_hunk_fn hunk_cb,
	git_diff_line_fn line_cb);

GIT_END_DECL

/** @} */

#endif
