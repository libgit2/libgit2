#ifndef __CLAR_LIBGIT2__
#define __CLAR_LIBGIT2__

#include "clar.h"
#include <git2.h>
#include "common.h"

/**
 * Replace for `clar_must_pass` that passes the last library error as the
 * test failure message.
 *
 * Use this wrapper around all `git_` library calls that return error codes!
 */
#define cl_git_pass(expr) do { \
	int _lg2_error; \
	giterr_clear(); \
	if ((_lg2_error = (expr)) != 0) \
		cl_git_report_failure(_lg2_error, __FILE__, __LINE__, "Function call failed: " #expr); \
	} while (0)

/**
 * Wrapper for `clar_must_fail` -- this one is
 * just for consistency. Use with `git_` library
 * calls that are supposed to fail!
 */
#define cl_git_fail(expr) cl_must_fail(expr)

#define cl_git_fail_with(expr, error) cl_assert_equal_i(error,expr)

void cl_git_report_failure(int, const char *, int, const char *);

#define cl_assert_equal_sz(sz1,sz2) cl_assert_equal_i((int)sz1, (int)(sz2))

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
void cl_git_write2file(const char *filename, const char *new_content, int flags, unsigned int mode);

bool cl_toggle_filemode(const char *filename);
bool cl_is_chmod_supported(void);

/* Environment wrappers */
char *cl_getenv(const char *name);
int cl_setenv(const char *name, const char *value);

/* Reliable rename */
int cl_rename(const char *source, const char *dest);

/* Git sandbox setup helpers */

git_repository *cl_git_sandbox_init(const char *sandbox);
void cl_git_sandbox_cleanup(void);

/* Local-repo url helpers */
const char* cl_git_fixture_url(const char *fixturename);
const char* cl_git_path_url(const char *path);

/* Test repository cleaner */
int cl_git_remove_placeholders(const char *directory_path, const char *filename);

/* config setting helpers */
void cl_repo_set_bool(git_repository *repo, const char *cfg, int value);

#endif
