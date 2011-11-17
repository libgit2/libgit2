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
	} while(0);

/**
 * Wrapper for `clay_must_fail` -- this one is
 * just for consistency. Use with `git_` library
 * calls that are supposed to fail!
 */
#define cl_git_fail(expr) cl_must_fail(expr)

#endif
