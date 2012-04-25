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

static struct {
	int num;
	const char *str;
} error_codes[] = {
	{GIT_ERROR, "Unspecified error"},
	{GIT_ENOTOID, "Input was not a properly formatted Git object id."},
	{GIT_ENOTFOUND, "Object does not exist in the scope searched."},
	{GIT_ENOMEM, "Not enough space available."},
	{GIT_EOSERR, "Consult the OS error information."},
	{GIT_EOBJTYPE, "The specified object is of invalid type"},
	{GIT_EOBJCORRUPTED, "The specified object has its data corrupted"},
	{GIT_ENOTAREPO, "The specified repository is invalid"},
	{GIT_EINVALIDTYPE, "The object or config variable type is invalid or doesn't match"},
	{GIT_EMISSINGOBJDATA, "The object cannot be written that because it's missing internal data"},
	{GIT_EPACKCORRUPTED, "The packfile for the ODB is corrupted"},
	{GIT_EFLOCKFAIL, "Failed to adquire or release a file lock"},
	{GIT_EZLIB, "The Z library failed to inflate/deflate an object's data"},
	{GIT_EBUSY, "The queried object is currently busy"},
	{GIT_EINVALIDPATH, "The path is invalid"},
	{GIT_EBAREINDEX, "The index file is not backed up by an existing repository"},
	{GIT_EINVALIDREFNAME, "The name of the reference is not valid"},
	{GIT_EREFCORRUPTED, "The specified reference has its data corrupted"},
	{GIT_ETOONESTEDSYMREF, "The specified symbolic reference is too deeply nested"},
	{GIT_EPACKEDREFSCORRUPTED, "The pack-refs file is either corrupted of its format is not currently supported"},
	{GIT_EINVALIDPATH, "The path is invalid" },
	{GIT_EREVWALKOVER, "The revision walker is empty; there are no more commits left to iterate"},
	{GIT_EINVALIDREFSTATE, "The state of the reference is not valid"},
	{GIT_ENOTIMPLEMENTED, "This feature has not been implemented yet"},
	{GIT_EEXISTS, "A reference with this name already exists"},
	{GIT_EOVERFLOW, "The given integer literal is too large to be parsed"},
	{GIT_ENOTNUM, "The given literal is not a valid number"},
	{GIT_EAMBIGUOUS, "The given oid prefix is ambiguous"},
};

const char *git_strerror(int num)
{
	size_t i;

	if (num == GIT_EOSERR)
		return strerror(errno);
	for (i = 0; i < ARRAY_SIZE(error_codes); i++)
		if (num == error_codes[i].num)
			return error_codes[i].str;

	return "Unknown error";
}

#define ERROR_MAX_LEN 1024

void git___rethrow(const char *msg, ...)
{
	char new_error[ERROR_MAX_LEN];
	char *last_error;
	char *old_error = NULL;

	va_list va;

	last_error = GIT_GLOBAL->error.last;

	va_start(va, msg);
	vsnprintf(new_error, ERROR_MAX_LEN, msg, va);
	va_end(va);

	old_error = git__strdup(last_error);

	snprintf(last_error, ERROR_MAX_LEN, "%s \n	- %s", new_error, old_error);

	git__free(old_error);
}

void git___throw(const char *msg, ...)
{
	va_list va;

	va_start(va, msg);
	vsnprintf(GIT_GLOBAL->error.last, ERROR_MAX_LEN, msg, va);
	va_end(va);
}

const char *git_lasterror(void)
{
	char *last_error = GIT_GLOBAL->error.last;

	if (!last_error[0]) {
		const git_error *err = git_error_last();
		if (err != NULL)
			return err->message;
		return NULL;
	}

	return last_error;
}

void git_clearerror(void)
{
	char *last_error = GIT_GLOBAL->error.last;
	last_error[0] = '\0';
}

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

const git_error *git_error_last(void)
{
	return GIT_GLOBAL->last_error;
}

void git_error_clear(void)
{
	giterr_clear();
}

