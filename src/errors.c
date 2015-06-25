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

/********************************************
 * New error handling
 ********************************************/

static const char *g_git_oom_error = "Out of memory";

static void set_error(char *string)
{
	git_global_st *global = GIT_GLOBAL;

	if (global->error_buf != string)
		git__free(global->error_buf);

	global->error_buf = string;
	global->last_error = string;
}

void giterr_set_oom(void)
{
	GIT_GLOBAL->last_error = g_git_oom_error;
}

void giterr_set(const char *fmt, ...)
{
	git_buf buf = GIT_BUF_INIT;
	va_list arglist;

	assert(fmt);

	va_start(arglist, fmt);
	git_buf_vprintf(&buf, fmt, arglist);
	va_end(arglist);

	if (!git_buf_oom(&buf))
		set_error(git_buf_detach(&buf));
}

void giterr_set_os(const char *fmt, ...)
{
	git_buf buf = GIT_BUF_INIT;
	va_list arglist;
	char *win32_error;
	int error_code = errno;

	assert(fmt);

	va_start(arglist, fmt);
	git_buf_vprintf(&buf, fmt, arglist);
	va_end(arglist);

	git_buf_PUTS(&buf, ": ");

#ifdef GIT_WIN32
	win32_error = git_win32_get_error_message(GetLastError());

	if (win32_error) {
		git_buf_puts(&buf, win32_error);
		git__free(win32_error);

		SetLastError(0);
	}
	else if (error_code) {
		git_buf_puts(&buf, strerror(error_code));
	}
#else
	if (error_code)
		git_buf_puts(&buf, strerror(error_code));
#endif

	if (!git_buf_oom(&buf))
		set_error(git_buf_detach(&buf));
}

int giterr_set_regex(const regex_t *regex, int error_code)
{
	char error_buf[1024];

	assert(error_code);

	regerror(error_code, regex, error_buf, sizeof(error_buf));
	giterr_set("%s", error_buf);

	if (error_code == REG_NOMATCH)
		return GIT_ENOTFOUND;

	return GIT_EINVALIDSPEC;
}

void giterr_clear(void)
{
	if (GIT_GLOBAL->last_error != NULL)
		set_error(NULL);

	errno = 0;
#ifdef GIT_WIN32
	SetLastError(0);
#endif
}

int giterr_is_oom(const char *e)
{
	return e == g_git_oom_error;
}

static int giterr_detach(const char **out)
{
	git_global_st *global = GIT_GLOBAL;
	const char *error = global->last_error;

	assert(out);

	if (!error)
		return -1;

	if (error == g_git_oom_error) {
		*out = g_git_oom_error;
		global->last_error = NULL;
	} else {
		*out = global->error_buf;
		global->error_buf = NULL;
	}

	giterr_clear();
	return 0;
}

const char *giterr_last(void)
{
	return GIT_GLOBAL->last_error;
}

int giterr_capture(git_error_state *state, int error_code)
{
	state->error_code = error_code;
	if (error_code)
		giterr_detach(&state->error_msg);
	return error_code;
}

int giterr_restore(git_error_state *state)
{
	int error;

	if (!state || !state->error_code || !state->error_msg) {
		giterr_clear();
		return 0;
	}

	if (state->error_msg == g_git_oom_error)
		giterr_set_oom();
	else
		set_error(state->error_msg);

	error = state->error_code;

	state->error_code = 0;
	state->error_msg = NULL;

	return error;
}

int giterr_system_last(void)
{
#ifdef GIT_WIN32
	return GetLastError();
#else
	return errno;
#endif
}

void giterr_system_set(int code)
{
#ifdef GIT_WIN32
	SetLastError(code);
#else
	errno = code;
#endif
}
