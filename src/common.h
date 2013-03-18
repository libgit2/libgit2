/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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
# include "win32/error.h"
# ifdef GIT_THREADS
#	include "win32/pthread.h"
#endif

#else

# include <unistd.h>
# ifdef GIT_THREADS
#	include <pthread.h>
# endif
#define GIT_STDLIB_CALL

#endif

#include "git2/types.h"
#include "git2/errors.h"
#include "thread-utils.h"
#include "bswap.h"

#include <regex.h>

/**
 * Check a pointer allocation result, returning -1 if it failed.
 */
#define GITERR_CHECK_ALLOC(ptr) if (ptr == NULL) { return -1; }

/**
 * Check a return value and propogate result if non-zero.
 */
#define GITERR_CHECK_ERROR(code) \
	do { int _err = (code); if (_err < 0) return _err; } while (0)

/**
 * Set the error message for this thread, formatting as needed.
 */
void giterr_set(int error_class, const char *string, ...);

/**
 * Set the error message for a regex failure, using the internal regex
 * error code lookup and return a libgit error code.
 */
int giterr_set_regex(const regex_t *regex, int error_code);

/**
 * Check a versioned structure for validity
 */
GIT_INLINE(int) giterr__check_version(const void *structure, unsigned int expected_max, const char *name)
{
	unsigned int actual;

	if (!structure)
		return 0;

	actual = *(const unsigned int*)structure;
	if (actual > 0 && actual <= expected_max)
		return 0;

	giterr_set(GITERR_INVALID, "Invalid version %d on %s", actual, name);
	return -1;
}
#define GITERR_CHECK_VERSION(S,V,N) if (giterr__check_version(S,V,N) < 0) return -1

/**
 * Initialize a structure with a version.
 */
GIT_INLINE(void) git__init_structure(void *structure, size_t len, unsigned int version)
{
	memset(structure, 0, len);
	*((int*)structure) = version;
}
#define GIT_INIT_STRUCTURE(S,V) git__init_structure(S, sizeof(*S), V)

/* NOTE: other giterr functions are in the public errors.h header file */

#include "util.h"

#endif /* INCLUDE_common_h__ */
