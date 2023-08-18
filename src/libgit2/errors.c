/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "threadstate.h"
#include "posix.h"
#include "str.h"
#include "libgit2.h"

/********************************************
 * New error handling
 ********************************************/

static git_error oom_error = {
	"Out of memory",
	GIT_ERROR_NOMEMORY
};

static git_error uninitialized_error = {
	"libgit2 has not been initialized; you must call git_libgit2_init",
	GIT_ERROR_INVALID
};

static git_error tlsdata_error = {
	"thread-local data initialization failure",
	GIT_ERROR_THREAD
};

static git_error no_error = {
	"no error",
	GIT_ERROR_NONE
};

#define IS_STATIC_ERROR(err) \
	((err) == &oom_error || (err) == &uninitialized_error || \
	 (err) == &tlsdata_error || (err) == &no_error)

static void set_error_from_buffer(int error_class)
{
	git_threadstate *threadstate = git_threadstate_get();
	git_error *error;
	git_str *buf;

	if (!threadstate)
		return;

	error = &threadstate->error_t;
	buf = &threadstate->error_buf;

	error->message = buf->ptr;
	error->klass = error_class;

	threadstate->last_error = error;
}

static void set_error(int error_class, char *string)
{
	git_threadstate *threadstate = git_threadstate_get();
	git_str *buf;

	if (!threadstate)
		return;

	buf = &threadstate->error_buf;

	git_str_clear(buf);

	if (string)
		git_str_puts(buf, string);

	if (!git_str_oom(buf))
		set_error_from_buffer(error_class);
}

void git_error_set_oom(void)
{
	git_threadstate *threadstate = git_threadstate_get();

	if (!threadstate)
		return;

	threadstate->last_error = &oom_error;
}

void git_error_set(int error_class, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	git_error_vset(error_class, fmt, ap);
	va_end(ap);
}

void git_error_vset(int error_class, const char *fmt, va_list ap)
{
#ifdef GIT_WIN32
	DWORD win32_error_code = (error_class == GIT_ERROR_OS) ? GetLastError() : 0;
#endif

	git_threadstate *threadstate = git_threadstate_get();
	int error_code = (error_class == GIT_ERROR_OS) ? errno : 0;
	git_str *buf;

	if (!threadstate)
		return;

	buf = &threadstate->error_buf;

	git_str_clear(buf);

	if (fmt) {
		git_str_vprintf(buf, fmt, ap);
		if (error_class == GIT_ERROR_OS)
			git_str_PUTS(buf, ": ");
	}

	if (error_class == GIT_ERROR_OS) {
#ifdef GIT_WIN32
		char *win32_error = git_win32_get_error_message(win32_error_code);
		if (win32_error) {
			git_str_puts(buf, win32_error);
			git__free(win32_error);

			SetLastError(0);
		}
		else
#endif
		if (error_code)
			git_str_puts(buf, strerror(error_code));

		if (error_code)
			errno = 0;
	}

	if (!git_str_oom(buf))
		set_error_from_buffer(error_class);
}

int git_error_set_str(int error_class, const char *string)
{
	git_threadstate *threadstate = git_threadstate_get();
	git_str *buf;

	GIT_ASSERT_ARG(string);

	if (!threadstate)
		return -1;

	buf = &threadstate->error_buf;

	git_str_clear(buf);
	git_str_puts(buf, string);

	if (git_str_oom(buf))
		return -1;

	set_error_from_buffer(error_class);
	return 0;
}

void git_error_clear(void)
{
	git_threadstate *threadstate = git_threadstate_get();

	if (!threadstate)
		return;

	if (threadstate->last_error != NULL) {
		set_error(0, NULL);
		threadstate->last_error = NULL;
	}

	errno = 0;
#ifdef GIT_WIN32
	SetLastError(0);
#endif
}

bool git_error_exists(void)
{
	git_threadstate *threadstate;

	if ((threadstate = git_threadstate_get()) == NULL)
		return true;

	return threadstate->last_error != NULL;
}

const git_error *git_error_last(void)
{
	git_threadstate *threadstate;

	/* If the library is not initialized, return a static error. */
	if (!git_libgit2_init_count())
		return &uninitialized_error;

	if ((threadstate = git_threadstate_get()) == NULL)
		return &tlsdata_error;

	if (!threadstate->last_error)
		return &no_error;

	return threadstate->last_error;
}

int git_error_save(git_error **out)
{
	git_threadstate *threadstate = git_threadstate_get();
	git_error *error, *dup;

	if (!threadstate) {
		*out = &tlsdata_error;
		return -1;
	}

	error = threadstate->last_error;

	if (!error || error == &no_error) {
		*out = &no_error;
		return 0;
	} else if (IS_STATIC_ERROR(error)) {
		*out = error;
		return 0;
	}

	if ((dup = git__malloc(sizeof(git_error))) == NULL) {
		*out = &oom_error;
		return -1;
	}

	dup->klass = error->klass;
	dup->message = git__strdup(error->message);

	if (!dup->message) {
		*out = &oom_error;
		return -1;
	}

	*out = dup;
	return 0;
}

int git_error_restore(git_error *error)
{
	git_threadstate *threadstate = git_threadstate_get();

	GIT_ASSERT_ARG(error);

	if (IS_STATIC_ERROR(error) && threadstate)
		threadstate->last_error = error;
	else
		set_error(error->klass, error->message);

	git_error_free(error);
	return 0;
}

void git_error_free(git_error *error)
{
	if (!error)
		return;

	if (IS_STATIC_ERROR(error))
		return;

	git__free(error->message);
	git__free(error);
}

int git_error_system_last(void)
{
#ifdef GIT_WIN32
	return GetLastError();
#else
	return errno;
#endif
}

void git_error_system_set(int code)
{
#ifdef GIT_WIN32
	SetLastError(code);
#else
	errno = code;
#endif
}

/* Deprecated error values and functions */

#ifndef GIT_DEPRECATE_HARD
const git_error *giterr_last(void)
{
	return git_error_last();
}

void giterr_clear(void)
{
	git_error_clear();
}

void giterr_set_str(int error_class, const char *string)
{
	git_error_set_str(error_class, string);
}

void giterr_set_oom(void)
{
	git_error_set_oom();
}
#endif
