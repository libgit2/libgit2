/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_diff_patch_h__
#define INCLUDE_diff_patch_h__

#include "common.h"
#include "diff.h"
#include "diff_file.h"
#include "array.h"

/* cached information about a single span in a diff */
typedef struct diff_patch_line diff_patch_line;
struct diff_patch_line {
	const char *ptr;
	size_t len;
	size_t lines, oldno, newno;
	char origin;
};

/* cached information about a hunk in a diff */
typedef struct diff_patch_hunk diff_patch_hunk;
struct diff_patch_hunk {
	git_diff_range range;
	char   header[128];
	size_t header_len;
	size_t line_start;
	size_t line_count;
};

struct git_diff_patch {
	git_refcount rc;
	git_diff_list *diff; /* for refcount purposes, maybe NULL for blob diffs */
	git_diff_delta *delta;
	size_t delta_index;
	git_diff_file_content ofile;
	git_diff_file_content nfile;
	uint32_t flags;
	git_array_t(diff_patch_hunk) hunks;
	git_array_t(diff_patch_line) lines;
	size_t oldno, newno;
	size_t content_size;
	git_pool flattened;
};

enum {
	GIT_DIFF_PATCH_ALLOCATED   = (1 << 0),
	GIT_DIFF_PATCH_INITIALIZED = (1 << 1),
	GIT_DIFF_PATCH_LOADED      = (1 << 2),
	GIT_DIFF_PATCH_DIFFABLE    = (1 << 3),
	GIT_DIFF_PATCH_DIFFED      = (1 << 4),
	GIT_DIFF_PATCH_FLATTENED   = (1 << 5),
};

typedef struct git_diff_output git_diff_output;
struct git_diff_output {
	/* these callbacks are issued with the diff data */
	git_diff_file_cb file_cb;
	git_diff_hunk_cb hunk_cb;
	git_diff_data_cb data_cb;
	void *payload;

	/* this records the actual error in cases where it may be obscured */
	int error;

	/* this callback is used to do the diff and drive the other callbacks.
	 * see diff_xdiff.h for how to use this in practice for now.
	 */
	int (*diff_cb)(git_diff_output *output, git_diff_patch *patch);
};

#endif
