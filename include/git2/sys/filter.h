/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_filter_h__
#define INCLUDE_sys_git_filter_h__

#include "git2/common.h"
#include "git2/types.h"

/**
 * @file git2/sys/filter.h
 * @brief Git custom filter implementors functions
 * @defgroup git_backend Git custom backend APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef enum {
	GIT_FILTER_TO_WORKDIR,
	GIT_FILTER_TO_ODB
} git_filter_mode_t;

/**
 * An instance for a custom filter
 */
struct git_filter {
	unsigned int version;

	/*
	 * determines whether or not a transformation should be applied for
	 * a specific file.  this function should return an error code
	 * (negative value) on error, 0 if the transformation should not be
	 * applied or 1 if the transformation should be applied.
	 */
	int (*should_apply)(git_filter *, const char *, git_filter_mode_t);

	/*
	 * apply a transformation on an input buffer.  this function should
	 * return an error code (negative value) on error, 0 if no
	 * transformation was applied, and 1 if a transformation was
	 * applied.
	 */
	int (*apply)(
		void **, size_t *, git_filter *, const char *, git_filter_mode_t mode, const void *, size_t);

	void (*free_buf)(void *);

	void (*free)(git_filter *);
};

#define GIT_FILTER_VERSION 1

/**
 * The default filters for a repository
 */

GIT_EXTERN(int) git_filter_crlf_new(git_filter **out, git_repository *repo);

GIT_END_DECL

#endif
