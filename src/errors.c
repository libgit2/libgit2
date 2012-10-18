/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "global.h"
#include "posix.h"
#include "buffer.h"
#include <stdarg.h>
#if GIT_WINHTTP
# include <winhttp.h>
# pragma comment(lib, "winhttp.lib")
#endif

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

	int unix_error_code = 0;

#ifdef GIT_WIN32
	DWORD win32_error_code = 0;
#endif

	if (error_class == GITERR_OS) {
		unix_error_code = errno;
		errno = 0;

#ifdef GIT_WIN32
		win32_error_code = GetLastError();
		SetLastError(0);
#endif
	}

	va_start(arglist, string);
	git_buf_vprintf(&buf, string, arglist);
	va_end(arglist);

	/* automatically suffix strerror(errno) for GITERR_OS errors */
	if (unix_error_code != 0) {
		git_buf_PUTS(&buf, ": ");
		git_buf_puts(&buf, strerror(unix_error_code));
	}

#ifdef GIT_WIN32
	else if (win32_error_code != 0) {
		LPTSTR lpMsgBuf = NULL;

#ifdef GIT_WINHTTP
		/* WinHttp error codes exist in winhttp.dll rather than the system message table */
		if (win32_error_code >= WINHTTP_ERROR_BASE && win32_error_code <= WINHTTP_ERROR_LAST) {
			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_HMODULE |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				GetModuleHandle(TEXT("winhttp.dll")), win32_error_code, 0, (LPTSTR)&lpMsgBuf, 0, NULL);
		} else {
#endif
			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, win32_error_code, 0, (LPTSTR)&lpMsgBuf, 0, NULL);
#ifdef GIT_WINHTTP
		}
#endif

		if (lpMsgBuf) {
			git_buf_PUTS(&buf, ": ");
			git_buf_puts(&buf, lpMsgBuf);
			LocalFree(lpMsgBuf);
		}
	}
#endif

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

void giterr_set_regex(const regex_t *regex, int error_code)
{
	char error_buf[1024];
	regerror(error_code, regex, error_buf, sizeof(error_buf));
	giterr_set_str(GITERR_REGEX, error_buf);
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

