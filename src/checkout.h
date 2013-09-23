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
	git_vector conflicts;
	git_buf path;
	size_t workdir_len;
	unsigned int strategy;
	int can_symlink;
	bool reload_submodules;
	size_t total_steps;
	size_t completed_steps;
} checkout_data;

typedef struct {
	const git_index_entry *ancestor;
	const git_index_entry *ours;
	const git_index_entry *theirs;

	int name_collision:1,
		directoryfile:1,
		one_to_two:1;
} checkout_conflictdata;

enum {
	CHECKOUT_ACTION__NONE = 0,
	CHECKOUT_ACTION__REMOVE = 1,
	CHECKOUT_ACTION__UPDATE_BLOB = 2,
	CHECKOUT_ACTION__UPDATE_SUBMODULE = 4,
	CHECKOUT_ACTION__CONFLICT = 8,
	CHECKOUT_ACTION__UPDATE_CONFLICT = 16,
	CHECKOUT_ACTION__MAX = 16,
	CHECKOUT_ACTION__DEFER_REMOVE = 32,
	CHECKOUT_ACTION__REMOVE_AND_UPDATE =
		(CHECKOUT_ACTION__UPDATE_BLOB | CHECKOUT_ACTION__REMOVE),
};

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

void git_checkout__report_progress(
	checkout_data *data,
	const char *path);

int git_checkout__get_conflicts(checkout_data *data, git_iterator *workdir, git_vector *pathspec);
int git_checkout__conflicts(checkout_data *data);

#endif
