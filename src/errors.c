/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "global.h"
#include "posix.h"
#include <stdarg.h>

/********************************************
 * New error handling
 ********************************************/

static git_error g_git_oom_error = {
	"Out of memory",
	GITERR_NOMEMORY
};

void giterr_set_oom(void)
{
	GIT_GLOBAL->last_error = &g_git_oom_error;
}

void giterr_set(int error_class, const char *string, ...)
{
	char error_str[1024];
	va_list arglist;

	/* Grab errno before calling vsnprintf() so it won't be overwritten */
	const char *os_error_msg =
		(error_class == GITERR_OS && errno != 0) ? strerror(errno) : NULL;
#ifdef GIT_WIN32
	DWORD dwLastError = GetLastError();
#endif

	va_start(arglist, string);
	p_vsnprintf(error_str, sizeof(error_str), string, arglist);
	va_end(arglist);

	/* automatically suffix strerror(errno) for GITERR_OS errors */
	if (error_class == GITERR_OS) {
		if (os_error_msg != NULL) {
			strncat(error_str, ": ", sizeof(error_str));
			strncat(error_str, os_error_msg, sizeof(error_str));
			errno = 0; /* reset so same error won't be reported twice */
		}
#ifdef GIT_WIN32
		else if (dwLastError != 0) {
			LPVOID lpMsgBuf = NULL;

			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, dwLastError, 0, (LPTSTR) &lpMsgBuf, 0, NULL);

			if (lpMsgBuf) {
				strncat(error_str, ": ", sizeof(error_str));
				strncat(error_str, (const char *)lpMsgBuf, sizeof(error_str));
				LocalFree(lpMsgBuf);
			}

			SetLastError(0);
		}
#endif
	}

	giterr_set_str(error_class, error_str);
}

void giterr_set_str(int error_class, const char *string)
{
	git_error *error = &GIT_GLOBAL->error_t;

	git__free(error->message);

	error->message = git__strdup(string);
	error->klass = error_class;

	if (error->message == NULL) {
		giterr_set_oom();
		return;
	}

	GIT_GLOBAL->last_error = error;
}

void giterr_set_regex(const regex_t *regex, int error_code)
{
	char error_buf[1024];
	regerror(error_code, regex, error_buf, sizeof(error_buf));
	giterr_set_str(GITERR_REGEX, error_buf);
}

void giterr_clear(void)
{
	GIT_GLOBAL->last_error = NULL;
}

const git_error *giterr_last(void)
{
	return GIT_GLOBAL->last_error;
}

