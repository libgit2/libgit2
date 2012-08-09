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
# include <winsock2.h>
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

#define GIT_OID_HEX_ZERO "0000000000000000000000000000000000000000"

/**
 * Check a pointer allocation result, returning -1 if it failed.
 */
#define GITERR_CHECK_ALLOC(ptr) if (ptr == NULL) { return -1; }

/**
 * Set the error message for this thread, formatting as needed.
 */
void giterr_set(int error_class, const char *string, ...);

/**
 * Set the error message for a regex failure, using the internal regex
 * error code lookup.
 */
void giterr_set_regex(const regex_t *regex, int error_code);

/* NOTE: other giterr functions are in the public errors.h header file */

#include "util.h"

typedef struct git_transport git_transport;
typedef struct gitno_buffer gitno_buffer;

#endif /* INCLUDE_common_h__ */
