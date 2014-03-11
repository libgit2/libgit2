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

static int git_warning__send(
	git_warning *warning, int default_rval, const char *fmt, va_list ap)
{
	int error = 0;
	git_buf buf = GIT_BUF_INIT;
	git_warning_callback cb = _warning_cb;

	if (!cb)
		return default_rval;

	if (!(error = git_buf_vprintf(&buf, fmt, ap))) {
		warning->message = git_buf_cstr(&buf);
		error = cb(warning, default_rval, _warning_payload);
	}

	git_buf_free(&buf);

	return error;
}

int git_warn(
	git_warning_t type,
	int default_rval,
	const char *fmt,
	...)
{
	int error;
	va_list ap;
	git_warning warning;

	if (!_warning_cb)
		return default_rval;

	warning.type = type;

	va_start(ap, fmt);
	error = git_warning__send(&warning, default_rval, fmt, ap);
	va_end(ap);

	return error;
}

int git_warn_invalid_data(
	git_warning_t type,
	int default_rval,
	const char *data,
	int datalen,
	const char *fmt,
	...)
{
	int error;
	va_list ap;
	git_warning_invalid_data warning;

	if (!_warning_cb)
		return default_rval;

	warning.base.type = type;
	warning.invalid_data = git__strndup(data, datalen);
	GITERR_CHECK_ALLOC(warning.invalid_data);
	warning.invalid_data_len = datalen;

	va_start(ap, fmt);
	error = git_warning__send((git_warning *)&warning, default_rval, fmt, ap);
	va_end(ap);

	git__free((char *)warning.invalid_data);

	return error;
}
