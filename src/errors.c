/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "global.h"
#include "posix.h"
#include "buffer.h"
#include <stdarg.h>

/********************************************
 * New error handling
 ********************************************/

static git_error g_git_oom_error = {
	"Out of memory",
	GITERR_NOMEMORY
};

static void set_error(int error_class, char *string)
{
	git_error *error = &GIT_GLOBAL->error_t;

	git__free(error->message);

	error->message = string;
	error->klass = error_class;

	GIT_GLOBAL->last_error = error;
}

void giterr_set_oom(void)
{
	GIT_GLOBAL->last_error = &g_git_oom_error;
}

void giterr_set(int error_class, const char *string, ...)
{
	git_buf buf = GIT_BUF_INIT;
	va_list arglist;
#ifdef GIT_WIN32
	DWORD win32_error_code = (error_class == GITERR_OS) ? GetLastError() : 0;
#endif
	int error_code = (error_class == GITERR_OS) ? errno : 0;

	va_start(arglist, string);
	git_buf_vprintf(&buf, string, arglist);
	va_end(arglist);

	if (error_class == GITERR_OS) {
#ifdef GIT_WIN32
		char * win32_error = git_win32_get_error_message(win32_error_code);
		if (win32_error) {
			git_buf_PUTS(&buf, ": ");
			git_buf_puts(&buf, win32_error);
			git__free(win32_error);

			SetLastError(0);
		}
		else
#endif
		if (error_code) {
			git_buf_PUTS(&buf, ": ");
			git_buf_puts(&buf, strerror(error_code));
		}

		if (error_code)
			errno = 0;
	}

	if (!git_buf_oom(&buf))
		set_error(error_class, git_buf_detach(&buf));
}

void giterr_set_str(int error_class, const char *string)
{
	char *message;

	assert(string);

	message = git__strdup(string);

	if (message)
		set_error(error_class, message);
}

int giterr_set_regex(const regex_t *regex, int error_code)
{
	char error_buf[1024];

	assert(error_code);

	regerror(error_code, regex, error_buf, sizeof(error_buf));
	giterr_set_str(GITERR_REGEX, error_buf);

	if (error_code == REG_NOMATCH)
		return GIT_ENOTFOUND;

	return GIT_EINVALIDSPEC;
}

void giterr_clear(void)
{
	GIT_GLOBAL->last_error = NULL;

	errno = 0;
#ifdef GIT_WIN32
	SetLastError(0);
#endif
}

const git_error *giterr_last(void)
{
	return GIT_GLOBAL->last_error;
}

