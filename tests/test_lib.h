#ifndef __LIBGIT2_TEST_H__
#define __LIBGIT2_TEST_H__

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include <git2.h>

#define DECLARE_SUITE(SNAME) extern git_testsuite *libgit2_suite_##SNAME(void)
#define SUITE_NAME(SNAME) libgit2_suite_##SNAME

#define BEGIN_SUITE(SNAME) \
	git_testsuite *libgit2_suite_##SNAME(void) {\
		git_testsuite *_gitsuite = git_testsuite_new(#SNAME);

#define ADD_TEST(TNAME) \
	git_testsuite_add(_gitsuite, _gittest__##TNAME);

#define END_SUITE \
		return _gitsuite;\
	}

#define BEGIN_TEST(TNAME, DESC) \
	static void _gittest__##TNAME(git_test *_gittest) { \
		git_test__init(_gittest, #TNAME, DESC); \
		git_clearerror();\
		{\

#define END_TEST }}

typedef struct git_test git_test;
typedef struct git_testsuite git_testsuite;
typedef void (*git_testfunc)(git_test *);
typedef git_testsuite *(*libgit2_suite)(void);

void git_test__init(git_test *t, const char *name, const char *description);
void git_test__fail(git_test *tc, const char *file, int line, const char *message);
void git_test__assert(git_test *tc, const char *file, int line, const char *message, int condition);
void git_test__assert_pass(git_test *tc, const char *file, int line, const char *message, int ret_value);

#define must_pass(expr) git_test__assert_pass(_gittest, __FILE__, __LINE__, "Method failed: " #expr, (expr))
#define must_fail(expr) git_test__assert(_gittest, __FILE__, __LINE__, "Expected method to fail: " #expr, (expr) < 0)
#define must_be_true(expr) git_test__assert(_gittest, __FILE__, __LINE__, "Expression is not true: " #expr, !!(expr))

git_testsuite *git_testsuite_new(const char *name);
void git_testsuite_add(git_testsuite *ts, git_testfunc test);
int git_testsuite_run(git_testsuite *ts);

#endif

