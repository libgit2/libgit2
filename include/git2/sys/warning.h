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
	GIT_WARNING_INVALID_DATA__SIGNATURE_TIMESTAMP, /* default continue */
	GIT_WARNING_INVALID_DATA__SIGNATURE_TIMEZONE,  /* default continue */
	GIT_WARNING_INVALID_DATA__SIGNATURE_EMAIL_MISSING, /* default error */
	GIT_WARNING_INVALID_DATA__SIGNATURE_EMAIL_UNTERMINATED, /* error */
} git_warning_t;

/**
 * Base class for warnings
 */
typedef struct git_warning git_warning;
struct git_warning {
	git_warning_t type;
	const char *message;
};

/**
 * Subclass of warning for invalid data string
 */
typedef struct {
	git_warning base;
	const char *invalid_data;
	int invalid_data_len;
} git_warning_invalid_data;

/**
 * Type for warning callbacks.
 *
 * Using `git_warning_set_callback(cb, payload)` you can set a warning
 * callback function (and payload) that will be used to issue various
 * warnings when recoverable data problems are encountered inside libgit2.
 * It will be passed a warning structure describing the problem.
 *
 * @param warning A git_warning structure for the specific situation
 * @param default_rval Default return code (i.e. <0 if warning defaults
 *                     to error, 0 if defaults to no error)
 * @param payload The payload set when callback function was specified
 * @return 0 to continue, <0 to convert the warning to an error
 */
typedef int (*git_warning_callback)(
	git_warning *warning, int default_rval, void *payload);

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
