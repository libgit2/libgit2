/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/thread-utils.h" /* for GIT_TLS */
#include "thread-utils.h" /* for GIT_TLS */

#include <stdarg.h>

static GIT_TLS char g_last_error[1024];
static GIT_TLS int  g_last_error_code;

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
	{GIT_EAMBIGUOUSOIDPREFIX, "The given oid prefix is ambiguous"},
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

void git___rethrow(const char *msg, ...)
{
	char new_error[1024];
	char *old_error = NULL;

	va_list va;

	g_last_error_code = error;

	va_start(va, msg);
	vsnprintf(new_error, sizeof(new_error), msg, va);
	va_end(va);

	old_error = strdup(g_last_error);
	snprintf(g_last_error, sizeof(g_last_error), "%s \n	- %s", new_error, old_error);
	free(old_error);
}

void git___throw(const char *msg, ...)
{
	va_list va;

	g_last_error_code = error;

	va_start(va, msg);
	vsnprintf(g_last_error, sizeof(g_last_error), msg, va);
	va_end(va);
}

const char *git_lasterror(void)
{
	if (!g_last_error[0])
		return NULL;

	return g_last_error;
}

int git_lasterror_code(void)
{
	return g_last_error_code;
}

void git_clearerror(void)
{
	g_last_error[0] = '\0';
}
