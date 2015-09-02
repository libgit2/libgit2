/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef INCLUDE_fnmatch__compat_h__
#define INCLUDE_fnmatch__compat_h__

#include "common.h"

#define GIT_FNM_NOMATCH		1		/* Match failed. */
#define GIT_FNM_NOSYS		2		/* Function not supported (unused). */
#define	GIT_FNM_NORES		3		/* Out of resources */

#define GIT_FNM_NOESCAPE	0x01		/* Disable backslash escaping. */
#define GIT_FNM_PATHNAME	0x02		/* Slash must be matched by slash. */
#define GIT_FNM_PERIOD		0x04		/* Period must be matched by period. */
#define GIT_FNM_LEADING_DIR 0x08		/* Ignore /<tail> after Imatch. */
#define GIT_FNM_CASEFOLD	0x10		/* Case insensitive search. */

#define GIT_FNM_IGNORECASE	GIT_FNM_CASEFOLD
#define GIT_FNM_FILE_NAME	GIT_FNM_PATHNAME

/**
 * Behave in a way similar to fnmatch() on Linux, but is portable
 * (see man fnmatch).
 * For instance, can be useful for people wanting to implement their
 * own RefDB backend
 */
GIT_EXTERN(int) git_fnmatch(const char *pattern, const char *string, int flags);

#endif /* _FNMATCH_H */

