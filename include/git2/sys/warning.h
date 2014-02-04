/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_sys_warning_h__
#define INCLUDE_git_sys_warning_h__

GIT_BEGIN_DECL

/**
 * Type for warning callbacks.
 *
 * Using `git_libgit2_opts(GIT_OPT_SET_WARNING_CALLBACK, ...)` you can set
 * a warning callback function (and payload) that will be used to issue
 * various warnings when recoverable data problems are encountered inside
 * libgit2.  It will be passed several parameters describing the problem.
 *
 * @param payload The payload set when callback function was specified
 * @param klass The git_error_t value describing the module issuing the warning
 * @param message The message with the specific warning being issued
 * @param repo The repository involved (may be NULL if problem is in a
 *      system config file, not a repo config file)
 * @param otype The type of object with bad data if applicable - GIT_OBJ_ANY
 *		will be used if none of the other types actually apply
 * @param object The object and/or raw data involved which will vary depending
 *		on the specific warning message being issued
 * @return 0 to continue, <0 to convert the warning to an error
 */
typedef int (*git_warning_callback)(
	void *payload,
	git_error_t klass,
	const char *message,
	git_repository *repo,
	git_otype otype,
	const void *object);

GIT_END_DECL

#endif
