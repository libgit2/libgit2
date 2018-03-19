/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_apply_h__
#define INCLUDE_git_apply_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "diff.h"

/**
 * @file git2/apply.h
 * @brief Git patch application routines
 * @defgroup git_apply Git patch application routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Apply a `git_diff` to a `git_tree`, and return the resulting image
 * as an index.
 *
 * @param out the postimage of the application
 * @param repo the repository to apply
 * @param preimage the tree to apply the diff to
 * @param diff the diff to apply
 */
GIT_EXTERN(int) git_apply_to_tree(
	git_index **out,
	git_repository *repo,
	git_tree *preimage,
	git_diff *diff);

typedef enum {
	/**
	 * Apply the patch to the workdir, leaving the index untouched.
	 * This is the equivalent of `git apply` with no location argument.
	 */
	GIT_APPLY_LOCATION_WORKDIR = 0,

	/**
	 * Apply the patch to the index, leaving the working directory
	 * untouched.  This is the equivalent of `git apply --cached`.
	 */
	GIT_APPLY_LOCATION_INDEX = 1,

	/**
	 * Apply the patch to both the working directory and the index.
	 * This is the equivalent of `git apply --index`.
	 */
	GIT_APPLY_LOCATION_BOTH = 2,
} git_apply_location_t;

typedef struct {
	unsigned int version;

	git_apply_location_t location;
} git_apply_options;

#define GIT_APPLY_OPTIONS_VERSION 1
#define GIT_APPLY_OPTIONS_INIT {GIT_APPLY_OPTIONS_VERSION}

/**
 * Apply a `git_diff` to the given repository, making changes directly
 * in the working directory, the index, or both.
 *
 * @param repo the repository to apply to
 * @param diff the diff to apply
 * @param options the options for the apply (or null for defaults)
 */
GIT_EXTERN(int) git_apply(
	git_repository *repo,
	git_diff *diff,
	git_apply_options *options);

/** @} */
GIT_END_DECL
#endif
