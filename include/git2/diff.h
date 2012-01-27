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

typedef int (*git_diff_file_fn)(
	void *cb_data,
	const git_oid *old,
	const char *old_path,
	int old_mode,
	const git_oid *new, /* hashed object if from work tree */
	const char *new_path,
	int new_mode);

typedef int (*git_diff_hunk_fn)(
	void *cb_data,
	int old_start,
	int old_lines,
	int new_start,
	int new_lines);

#define GIT_DIFF_LINE_CONTEXT  0
#define GIT_DIFF_LINE_ADDITION 1
#define GIT_DIFF_LINE_DELETION 2

typedef int (*git_diff_line_fn)(
	void *cb_data,
	int origin, /* GIT_DIFF_LINE value from above */
	const char *content,
	size_t content_len);

typedef struct {
	int context_lines;
	int interhunk_lines;
	int ignore_whitespace;

	git_diff_file_fn file_cb;
	git_diff_hunk_fn hunk_cb;
	git_diff_line_fn line_cb;
	void *cb_data;
} git_diff_opts;


GIT_EXTERN(int) git_diff_blobs(
	git_repository *repo,
	git_blob *old,
	git_blob *new,
	git_diff_opts *options);

GIT_EXTERN(int) git_diff_trees(
	git_repository *repo,
	git_tree *old,
	git_tree *new,
	git_diff_opts *options);

GIT_EXTERN(int) git_diff_index(
	git_repository *repo,
	git_tree *old,
	git_diff_opts *options);

/* pass NULL for the git_tree to diff workdir against index */
GIT_EXTERN(int) git_diff_workdir(
	git_repository *repo,
	git_tree *old,
	git_diff_opts *options);

GIT_EXTERN(int) git_diff_workdir_file(
	git_repository *repo,
	git_blob *old,
	const char *path,
	git_diff_opts *options);

/* pass git_objects to diff against or NULL for index.
 * can handle: blob->blob, tree->index, tree->tree
 * it will be an error if object types don't match
 */
/* pass git_object to diff WT against or NULL for index
 * can handle: index->wt, tree->wt, blob->wt with path
 */

GIT_END_DECL

/** @} */

#endif
