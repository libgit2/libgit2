#ifndef __CLAR_LIBGIT2__
#define __CLAR_LIBGIT2__

#include "clar.h"
#include <git2.h>
#include "common.h"

/**
 * Special wrapper for `clar_must_pass` that passes
 * the last library error as the test failure message.
 *
 * Use this wrapper around all `git_` library calls that
 * return error codes!
 */
#define cl_git_pass(expr) do { \
	git_clearerror(); \
	if ((expr) != GIT_SUCCESS) \
		clar__assert(0, __FILE__, __LINE__, "Function call failed: " #expr, git_lasterror(), 1); \
	} while(0)

/**
 * Wrapper for `clar_must_fail` -- this one is
 * just for consistency. Use with `git_` library
 * calls that are supposed to fail!
 */
#define cl_git_fail(expr) cl_must_fail(expr)

/**
 * Wrapper for string comparison that knows about nulls.
 */
#define cl_assert_strequal(a,b) \
	cl_assert_strequal_internal(a,b,__FILE__,__LINE__,"string mismatch: " #a " != " #b)

GIT_INLINE(void) cl_assert_strequal_internal(
	const char *a, const char *b, const char *file, int line, const char *err)
{
	int match = (a == NULL || b == NULL) ? (a == b) : (strcmp(a, b) == 0);
	if (!match) {
		char buf[4096];
		snprintf(buf, 4096, "'%s' != '%s'", a, b);
		clar__assert(0, file, line, buf, err, 1);
	}
}

/*
 * Some utility macros for building long strings
 */
#define REP4(STR)	 STR STR STR STR
#define REP15(STR)	 REP4(STR) REP4(STR) REP4(STR) STR STR STR
#define REP16(STR)	 REP4(REP4(STR))
#define REP256(STR)  REP16(REP16(STR))
#define REP1024(STR) REP4(REP256(STR))

/* Write the contents of a buffer to disk */
void cl_git_mkfile(const char *filename, const char *content);

#endif
