/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_warning_h__
#define INCLUDE_git_warning_h__

#include "common.h"

/**
 * @file git2/warning.h
 * @brief Git warning routines
 * @defgroup git_warning Git warning routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * The warning type.
 */
typedef enum {
	/**
	 * A warning provided when safe directory handling is invoked.
	 *
	 * The data will be:
	 *
	 * - `const char *path`: the path to the repository
	 */
	GIT_WARNING_SAFE_DIRECTORY = 1
} git_warning_t;

typedef enum {
	/*
	 * Instructs libgit2 to continue its normal error handling in this
	 * case.
	 */
	GIT_WARNING_CONTINUE = 0,

	/*
	 * Instructs libgit2 to ignore this warning and continue as if it
	 * did not happen.
	 */
	GIT_WARNING_IGNORE = 1
} git_warning_result_t;

/** @} */
GIT_END_DECL
#endif
