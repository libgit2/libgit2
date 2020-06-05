/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_sys_git_errors_h__
#define INCLUDE_sys_git_errors_h__

#include "git2/common.h"

/**
 * @file git2/sys/errors.h
 * @brief Advanced Git error handling routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Set the error message string for this thread using `printf`-style
 * formatting.  This function is public for interoperability with
 * the client library and has several caveats:
 *
 * - The given format string will be provided to the system's `printf`
 *   library, so inputs may not be portable across platforms.
 * - There is no validation checking on the given inputs.
 * - In an out of memory situation caused by formatting the error
 *   message, the given error message will not be used.  There is no
 *   return value to indicate this situation occurred.
 *
 * If you need to set the error message (for example, because you're
 * writing an ODB backend) then you should use `git_error_set_str`.
 *
 * @param error_class A `git_error_t` describing the subsystem
 * @param fmt The formatted error message to keep
 * @param ... Formatting values
 */
GIT_EXTERN(void) git_error_set(int error_class, const char *fmt, ...) GIT_FORMAT_PRINTF(2, 3);

/** @} */
GIT_END_DECL
#endif
