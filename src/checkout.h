/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_checkout_h__
#define INCLUDE_checkout_h__

#include "git2/checkout.h"
#include "iterator.h"
#include "pool.h"

#define GIT_CHECKOUT__NOTIFY_CONFLICT_TREE (1u << 12)

typedef struct {
	git_repository *repo;
	git_diff_list *diff;
	git_checkout_opts opts;
	bool opts_free_baseline;
	char *pfx;
	git_index *index;
	git_pool pool;
	git_vector removes;
	git_buf path;
	size_t workdir_len;
	unsigned int strategy;
	int can_symlink;
	bool reload_submodules;
	size_t total_steps;
	size_t completed_steps;
} checkout_data;

/**
 * Update the working directory to match the target iterator.  The
 * expected baseline value can be passed in via the checkout options
 * or else will default to the HEAD commit.
 */
extern int git_checkout_iterator(
	git_iterator *target,
	const git_checkout_opts *opts);

int git_checkout__safe_for_update_only(
	const char *path,
	mode_t expected_mode);

int git_checkout__write_content(
	checkout_data *data,
	const git_oid *oid,
	const char *path,
	const char *hint_path,
	unsigned int mode,
	struct stat *st);

int git_checkout__conflicts(checkout_data *data);

#endif
