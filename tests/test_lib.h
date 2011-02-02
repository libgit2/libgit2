#ifndef __LIBGIT2_TEST_H__
#define __LIBGIT2_TEST_H__

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include <git2.h>

#define ADD_TEST(SUITE, MODULE, TEST) \
	git_testsuite_add(SUITE, git_test_new(MODULE "::" #TEST, &_gittest__##TEST))

#define BEGIN_TEST(MODULE, TEST) \
	void _gittest__##TEST(git_test *_gittest) \
	{ \
		assert(_gittest);\
		{\

#define END_TEST }}

typedef struct git_test git_test;
typedef struct git_testsuite git_testsuite;
typedef void (*git_testfunc)(git_test *);

void git_test__fail(git_test *tc, const char *file, int line, const char *message);
void git_test__assert(git_test *tc, const char *file, int line, const char *message, int condition);

#define must_pass(expr) git_test__assert(_gittest, __FILE__, __LINE__, "Method failed, " #expr, (expr) == 0)
#define must_fail(expr) git_test__assert(_gittest, __FILE__, __LINE__, "Expected method to fail, " #expr, (expr) < 0)
#define must_be_true(expr) git_test__assert(_gittest, __FILE__, __LINE__, "Expected " #expr, !!(expr))

git_testsuite *git_testsuite_new(const char *name);
git_test *git_test_new(const char *name, git_testfunc function);

void git_testsuite_free(git_testsuite *ts);

void git_testsuite_add(git_testsuite *ts, git_test *tc);
void git_testsuite_addsuite(git_testsuite* ts, git_testsuite *ts2);
int git_testsuite_run(git_testsuite *ts);

#endif

