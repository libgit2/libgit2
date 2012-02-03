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
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef struct {
	int context_lines;
	int interhunk_lines;
	int ignore_whitespace;
	int force_text;
	git_strarray pathspec;
} git_diff_options;

typedef struct {
	git_status_t status;     /* value from tree.h */
	unsigned int old_attr;
	unsigned int new_attr;
	git_oid      old_oid;
	git_oid      new_oid;
	git_blob     *old_blob;
	git_blob     *new_blob;
	const char   *path;
	const char   *new_path;  /* NULL unless status is RENAMED or COPIED */
	int          similarity; /* value from 0 to 100 */
	int          binary;     /* diff as binary? */
} git_diff_delta;

typedef int (*git_diff_file_fn)(
	void *cb_data,
	git_diff_delta *delta,
	float progress);

typedef struct {
	int old_start;
	int old_lines;
	int new_start;
	int new_lines;
} git_diff_range;

typedef int (*git_diff_hunk_fn)(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	const char *header,
	size_t header_len);

#define GIT_DIFF_LINE_CONTEXT	' '
#define GIT_DIFF_LINE_ADDITION	'+'
#define GIT_DIFF_LINE_DELETION	'-'
#define GIT_DIFF_LINE_ADD_EOFNL '\n'
#define GIT_DIFF_LINE_DEL_EOFNL '\0'

typedef int (*git_diff_line_fn)(
	void *cb_data,
	git_diff_delta *delta,
	char line_origin, /* GIT_DIFF_LINE value from above */
	const char *content,
	size_t content_len);

typedef struct git_diff_list git_diff_list;

/*
 * Generate diff lists
 */

GIT_EXTERN(int) git_diff_tree_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_tree *new,
	git_diff_list **diff);

GIT_EXTERN(int) git_diff_index_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff);

GIT_EXTERN(int) git_diff_workdir_to_tree(
	git_repository *repo,
	const git_diff_options *opts,
	git_tree *old,
	git_diff_list **diff);

GIT_EXTERN(int) git_diff_workdir_to_index(
	git_repository *repo,
	const git_diff_options *opts,
	git_diff_list **diff);

GIT_EXTERN(void) git_diff_list_free(git_diff_list *diff);

/*
 * Process diff lists
 */

GIT_EXTERN(int) git_diff_foreach(
	git_diff_list *diff,
	void *cb_data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_line_fn line_cb);

#ifndef _STDIO_H_
#include <stdio.h>
#endif

GIT_EXTERN(int) git_diff_print_compact(
	FILE *fp, git_diff_list *diff);

GIT_EXTERN(int) git_diff_print_patch(
	FILE *fp, git_diff_list *diff);

/*
 * Misc
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
