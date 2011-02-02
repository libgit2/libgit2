#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "test_lib.h"

#define DO_ALLOC(TYPE) ((TYPE*) malloc(sizeof(TYPE)))
#define GIT_MAX_TEST_CASES 64

struct git_test {
	char *name;
	git_testfunc function;
	int failed;
	int ran;
	const char *message;
	jmp_buf *jump;
};

struct git_testsuite {
	char *name;
	int count, fail_count;
	git_test *list[GIT_MAX_TEST_CASES];
};

static void test_init(git_test *t, const char *name, git_testfunc function)
{
	t->name = strdup(name);
	t->failed = 0;
	t->ran = 0;
	t->message = NULL;
	t->function = function;
	t->jump = NULL;
}

static void test_free(git_test *t)
{
	if (t) {
		free(t->name);
		free(t);
	}
}

void test_run(git_test *tc)
{
	jmp_buf buf;
	tc->jump = &buf;

	if (setjmp(buf) == 0) {
		tc->ran = 1;
		(tc->function)(tc);
	}

	tc->jump = 0;
}

git_test *git_test_new(const char *name, git_testfunc function)
{
	git_test *tc = DO_ALLOC(git_test);
	test_init(tc, name, function);
	return tc;
}


/*-------------------------------------------------------------------------*
 * Public assert methods
 *-------------------------------------------------------------------------*/

static void fail_test(git_test *tc, const char *file, int line, const char *message)
{
	char buf[1024];

	snprintf(buf, 1024, "%s @ %s:%d", message, file, line);

	tc->failed = 1;
	tc->message = strdup(buf);

	if (tc->jump != 0)
		longjmp(*(tc->jump), 0);
}

void git_test__fail(git_test *tc, const char *file, int line, const char *message)
{
	fail_test(tc, file, line, message);
}

void git_test__assert(git_test *tc, const char *file, int line, const char *message, int condition)
{
	if (condition == 0)
		fail_test(tc, file, line, message);
}

/*-------------------------------------------------------------------------*
 * Test Suite
 *-------------------------------------------------------------------------*/

static void testsuite_init(git_testsuite *ts)
{
	ts->count = 0;
	ts->fail_count = 0;
	memset(ts->list, 0, sizeof(ts->list));
}

git_testsuite *git_testsuite_new(const char *name)
{
	git_testsuite *ts = DO_ALLOC(git_testsuite);
	testsuite_init(ts);
	ts->name = strdup(name);
	return ts;
}

void git_testsuite_free(git_testsuite *ts)
{
	unsigned int n;

	for (n = 0; n < GIT_MAX_TEST_CASES; n++)
		if (ts->list[n])
			test_free(ts->list[n]);

	free(ts);
}

void git_testsuite_add(git_testsuite *ts, git_test *tc)
{
	assert(ts->count < GIT_MAX_TEST_CASES);
	ts->list[ts->count++] = tc;
}

void git_testsuite_addsuite(git_testsuite *ts, git_testsuite *ts2)
{
	int i;
	for (i = 0 ; i < ts2->count ; ++i)
		git_testsuite_add(ts, ts2->list[i]);
}


static void print_details(git_testsuite *ts)
{
	int i;
	int failCount = 0;

	if (ts->fail_count == 0) {
		const char *testWord = ts->count == 1 ? "test" : "tests";
		printf("OK (%d %s)\n", ts->count, testWord);
	} else {
		printf("Failed (%d failures):\n", ts->fail_count);

		for (i = 0 ; i < ts->count ; ++i) {
			git_test *tc = ts->list[i];
			if (tc->failed) {
				failCount++;
				printf("  %d) %s: %s\n", failCount, tc->name, tc->message);
			}
		}
	}
}

int git_testsuite_run(git_testsuite *ts)
{
	int i;

	printf("Suite \"%s\": ", ts->name);

	for (i = 0 ; i < ts->count ; ++i) {
		git_test *tc = ts->list[i];

		test_run(tc);
		if (tc->failed) {
			ts->fail_count++;
			putchar('F');
		} else
			putchar('.');
	}
	printf("\n  ");
	print_details(ts);

	return ts->fail_count;
}

