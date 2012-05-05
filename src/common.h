/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#include "git2/common.h"
#include "cc-compat.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef GIT_WIN32

# include <io.h>
# include <direct.h>
# include <windows.h>
# include "win32/msvc-compat.h"
# include "win32/mingw-compat.h"
# ifdef GIT_THREADS
#	include "win32/pthread.h"
#endif

# define snprintf _snprintf

#else
# include <unistd.h>

# ifdef GIT_THREADS
#	include <pthread.h>
# endif
#endif

#include "git2/types.h"
#include "git2/errors.h"
#include "thread-utils.h"
#include "bswap.h"

#include <regex.h>

extern void git___throw(const char *, ...) GIT_FORMAT_PRINTF(1, 2);
#define git__throw(error, ...) \
	(git___throw(__VA_ARGS__), error)

extern void git___rethrow(const char *, ...) GIT_FORMAT_PRINTF(1, 2);
#define git__rethrow(error, ...) \
	(git___rethrow(__VA_ARGS__), error)


#define GITERR_CHECK_ALLOC(ptr) if (ptr == NULL) { return -1; }

void giterr_set_oom(void);
void giterr_set(int error_class, const char *string, ...);
void giterr_clear(void);
void giterr_set_str(int error_class, const char *string);
void giterr_set_regex(const regex_t *regex, int error_code);


#include "util.h"


#endif /* INCLUDE_common_h__ */
