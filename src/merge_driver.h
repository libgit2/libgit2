/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_merge_driver_h__
#define INCLUDE_merge_driver_h__

#include "git2/merge.h"
#include "git2/index.h"
#include "git2/sys/merge.h"

struct git_merge_driver_source {
	git_repository *repo;
	const char *default_driver;
	const git_merge_file_options *file_opts;

	const git_index_entry *ancestor;
	const git_index_entry *ours;
	const git_index_entry *theirs;
};

extern int git_merge_driver_global_init(void);

extern int git_merge_driver_for_path(
	char **name_out,
	git_merge_driver **driver_out,
	git_repository *repo,
	const char *path);

/* Basic (normal) merge driver, takes favor type as the payload argument */
extern git_merge_driver git_merge_driver__normal;

/* Merge driver for text files, performs a standard three-way merge */
extern git_merge_driver git_merge_driver__text;

/* Merge driver for union-style merging */
extern git_merge_driver git_merge_driver__union;

/* Merge driver for unmergeable (binary) files: always produces conflicts */
extern git_merge_driver git_merge_driver__binary;

#endif
