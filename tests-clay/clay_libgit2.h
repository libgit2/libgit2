#ifndef __CLAY_LIBGIT2__
#define __CLAY_LIBGIT2__

#include "clay.h"
#include <git2.h>
#include "common.h"

/**
 * Special wrapper for `clay_must_pass` that passes
 * the last library error as the test failure message.
 *
 * Use this wrapper around all `git_` library calls that
 * return error codes!
 */
#define cl_git_pass(expr) do { \
	git_clearerror(); \
	if ((expr) != GIT_SUCCESS) \
		clay__assert(0, __FILE__, __LINE__, "Function call failed: " #expr, git_lasterror(), 1); \
	} while(0)

/**
 * Wrapper for `clay_must_fail` -- this one is
 * just for consistency. Use with `git_` library
 * calls that are supposed to fail!
 */
#define cl_git_fail(expr) cl_must_fail(expr)

/**
 * Wrapper for string comparison that knows about nulls.
 */
#define cl_assert_strequal(a,b) \
	cl_assert_strequal_internal(a,b,__FILE__,__LINE__)

GIT_INLINE(void) cl_assert_strequal_internal(const char *a, const char *b, const char *file, int line)
{
	int match = (a == NULL || b == NULL) ? (a == b) : (strcmp(a, b) == 0);
	if (!match) {
		char buf[4096];
		snprintf(buf, 4096, "'%s' != '%s'", a, b);
		clay__assert(0, file, line, buf, "Strings do not match", 1);
	}
}

#endif
