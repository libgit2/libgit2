/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include "cli.h"

#define printerr(fmt, giterr) do { \
	if (fmt) { \
		va_list ap; \
		va_start(ap, fmt); \
		__printerr(fmt, ap, giterr); \
		va_end(ap); \
	} else { \
		__printerr(NULL, NULL, giterr); \
	} \
} while(0)

GIT_INLINE(void) __printerr(
	const char *fmt, va_list ap, const git_error *giterr)
{
	fprintf(stderr, "%s: ", PROGRAM_NAME);

	if (fmt)
		vfprintf(stderr, fmt, ap);

	if (fmt && giterr)
		fprintf(stderr, ": ");

	if (giterr)
		fprintf(stderr, "%s", giterr->message);

	if (!fmt && !giterr)
		fprintf(stderr, "(unknown error)");

	fprintf(stderr, "\n");
}

void cli_error(const char *fmt, ...)
{
	printerr(fmt, NULL);
}

void cli_error_git(const char *fmt, ...)
{
	printerr(fmt, git_error_last());
}

void cli_error_os(const char *fmt, ...)
{
	git_error_set(GIT_ERROR_OS, NULL);
	printerr(fmt, git_error_last());
}

