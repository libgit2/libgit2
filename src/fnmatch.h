/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_fnmatch__compat_h__
#define INCLUDE_fnmatch__compat_h__

#include "common.h"

#define FNM_NOMATCH		1		/* Match failed. */
#define FNM_NOSYS		2		/* Function not supported (unused). */
#define	FNM_NORES		3		/* Out of resources */

#define FNM_NOESCAPE	0x01		/* Disable backslash escaping. */
#define FNM_PATHNAME	0x02		/* Slash must be matched by slash. */
#define FNM_PERIOD		0x04		/* Period must be matched by period. */
#define FNM_LEADING_DIR 0x08		/* Ignore /<tail> after Imatch. */
#define FNM_CASEFOLD	0x10		/* Case insensitive search. */

#define FNM_IGNORECASE	FNM_CASEFOLD
#define FNM_FILE_NAME	FNM_PATHNAME

extern int p_fnmatch(const char *pattern, const char *string, int flags);

#endif /* _FNMATCH_H */

