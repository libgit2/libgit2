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

void giterr_set_oom(void)
{
	GIT_GLOBAL->last_error = g_git_oom_error;
}

static void giterr__set(const char *fmt, va_list arg, const char *suffix)
{
	git_global_st *global = GIT_GLOBAL;

	git_buf_clear(&global->error_buf);
	git_buf_vprintf(&global->error_buf, fmt, arg);

	if (suffix) {
		git_buf_put(&global->error_buf, ": ", 2);
		git_buf_puts(&global->error_buf, suffix);
	}

	global->last_error = git_buf_oom(&global->error_buf) ?
		g_git_oom_error : global->error_buf.ptr;
}

void giterr_set(const char *fmt, ...)
{
	va_list arglist;

	assert(fmt);

	va_start(arglist, fmt);
	giterr__set(fmt, arglist, NULL);
	va_end(arglist);
}

void giterr_set_os(const char *fmt, ...)
{
	char *win32_error = NULL, *suffix = NULL;
	va_list arglist;
	int error_code = errno;

	assert(fmt);

#ifdef GIT_WIN32
	win32_error = git_win32_get_error_message(GetLastError());

	if (win32_error) {
		suffix = win32_error;
		SetLastError(0);
	} else if (error_code) {
		suffix = strerror(error_code);
	}	
#else
	if (error_code)
		suffix = strerror(error_code);
#endif

	va_start(arglist, fmt);
	giterr__set(fmt, arglist, suffix);
	va_end(arglist);

	git__free(win32_error);
}

int giterr_set_regex(const regex_t *regex, int error_code)
{
	git_global_st *global = GIT_GLOBAL;
	char error_buf[1024];

	assert(error_code);

	regerror(error_code, regex, error_buf, sizeof(error_buf));

	git_buf_puts(&global->error_buf, error_buf);
	global->last_error = git_buf_oom(&global->error_buf) ?
		g_git_oom_error : global->error_buf.ptr;

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

int giterr_is_oom(const char *e)
{
	return e == g_git_oom_error;
}

const char *giterr_last(void)
{
	return GIT_GLOBAL->last_error;
}

int giterr_capture(git_error_state *state, int error_code)
{
	git_global_st *global = GIT_GLOBAL;

	state->error_code = error_code;

	if (error_code) {
		if (global->last_error == g_git_oom_error) {
			state->last_error = g_git_oom_error;
		} else {
			git_buf_free(&state->error_buf);
			git_buf_swap(&state->error_buf, &global->error_buf);
			state->last_error = state->error_buf.ptr;
		}

		global->last_error = NULL;
	}

	return error_code;
}

int giterr_restore(git_error_state *state)
{
	git_global_st *global = GIT_GLOBAL;
	int error;

	if (!state || !state->error_code || !state->last_error) {
		giterr_clear();
		return 0;
	}

	if (state->last_error == g_git_oom_error) {
		giterr_set_oom();
	} else {
		git_buf_free(&global->error_buf);
		git_buf_swap(&global->error_buf, &state->error_buf);

		global->last_error = global->error_buf.ptr;
	}

	error = state->error_code;

	state->error_code = 0;
	state->last_error = NULL;

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
