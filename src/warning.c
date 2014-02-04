/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "warning.h"
#include "buffer.h"
#include <stdarg.h>

static git_warning_callback _warning_cb = NULL;
static void *_warning_payload = NULL;

void git_warning_set_callback(git_warning_callback cb, void *payload)
{
	_warning_cb = cb;
	_warning_payload = payload;
}

int git_warning(
	git_error_t klass,
	git_repository *repo,
	git_otype otype,
	const void *object,
	const char *fmt,
	...)
{
	int error = 0;
	git_buf buf = GIT_BUF_INIT;
	git_warning_callback cb = _warning_cb;
	va_list arglist;

	if (!cb)
		return 0;

	va_start(arglist, fmt);
	error = git_buf_vprintf(&buf, fmt, arglist);
	va_end(arglist);

	if (!error)
		error = cb(
			_warning_payload, klass, git_buf_cstr(&buf), repo, otype, object);

	git_buf_free(&buf);

	return error;
}

