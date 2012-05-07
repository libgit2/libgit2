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
	giterr_clear(); \
	if ((expr) != 0) \
		clar__assert(0, __FILE__, __LINE__, "Function call failed: " #expr, giterr_last() ? giterr_last()->message : NULL, 1); \
	} while(0)

/**
 * Wrapper for `clar_must_fail` -- this one is
 * just for consistency. Use with `git_` library
 * calls that are supposed to fail!
 */
#define cl_git_fail(expr) cl_must_fail(expr)

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
void cl_git_append2file(const char *filename, const char *new_content);
void cl_git_rewritefile(const char *filename, const char *new_content);
void cl_git_write2file(const char *filename, const char *new_content, int mode);

/* Git sandbox setup helpers */

git_repository *cl_git_sandbox_init(const char *sandbox);
void cl_git_sandbox_cleanup(void);

#endif
