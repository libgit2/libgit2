/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_diff_file_h__
#define INCLUDE_diff_file_h__

#include "common.h"
#include "diff.h"
#include "diff_driver.h"
#include "map.h"

/* expanded information for one side of a delta */
typedef struct {
	git_repository *repo;
	const git_diff_options *opts;
	git_diff_file file;
	git_diff_driver *driver;
	git_iterator_type_t src;
	const git_blob *blob;
	git_map map;
} git_diff_file_content;

extern int diff_file_content_init_from_diff(
	git_diff_file_content *fc,
	git_diff_list *diff,
	size_t delta_index,
	bool use_old);

extern int diff_file_content_init_from_blob(
	git_diff_file_content *fc,
	git_repository *repo,
	const git_diff_options *opts,
	const git_blob *blob);

extern int diff_file_content_init_from_raw(
	git_diff_file_content *fc,
	git_repository *repo,
	const git_diff_options *opts,
	const char *buf,
	size_t buflen);

/* this loads the blob/file-on-disk as needed */
extern int diff_file_content_load(git_diff_file_content *fc);

/* this releases the blob/file-in-memory */
extern void diff_file_content_unload(git_diff_file_content *fc);

/* this unloads and also releases any other resources */
extern void diff_file_content_clear(git_diff_file_content *fc);

#endif
