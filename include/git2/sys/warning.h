/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_sys_warning_h__
#define INCLUDE_git_sys_warning_h__

GIT_BEGIN_DECL

typedef enum {
	GIT_WARNING_NONE = 0,
	GIT_WARNING_INVALID_SIGNATURE_TIMESTAMP,
	GIT_WARNING_INVALID_SIGNATURE_TIMEZONE,
} git_warning_t;

/**
 * Type for warning callbacks.
 *
 * Using `git_warning_set_callback(cb, payload)` you can set a warning
 * callback function (and payload) that will be used to issue various
 * warnings when recoverable data problems are encountered inside libgit2.
 * It will be passed several parameters describing the problem.
 *
 * @param warning A git_warning_t value for the specific situation
 * @param message A message explaining the details of the warning
 * @param payload The payload set when callback function was specified
 * @return 0 to continue, <0 to convert the warning to an error
 */
typedef int (*git_warning_callback)(
	git_warning_t warning,
	const char *message,
	void *payload);

/**
 * Set the callback to be invoked when an invalid but recoverable
 * scenario occurs.
 *
 * @param callback The git_warning_callback to be invoked
 * @param payload The payload parameter for the callback function
 */
GIT_EXTERN(void) git_warning_set_callback(
	git_warning_callback callback,
	void *payload);

GIT_END_DECL

#endif
