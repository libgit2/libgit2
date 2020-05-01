/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_util_errors_h__
#define INCLUDE_util_errors_h__

#include "git2_util.h"

/**
 * Set error message for user callback if needed.
 *
 * If the error code in non-zero and no error message is set, this
 * sets a generic error message.
 *
 * @return This always returns the `error_code` parameter.
 */
GIT_INLINE(int) git_error_set_after_callback_function(
	int error_code, const char *action)
{
	if (error_code) {
		const git_error *e = git_error_last();
		if (!e || !e->message)
			git_error_set(e ? e->klass : GIT_ERROR_CALLBACK,
				"%s callback returned %d", action, error_code);
	}
	return error_code;
}

#ifdef GIT_WIN32
#define git_error_set_after_callback(code) \
	git_error_set_after_callback_function((code), __FUNCTION__)
#else
#define git_error_set_after_callback(code) \
	git_error_set_after_callback_function((code), __func__)
#endif

#endif
