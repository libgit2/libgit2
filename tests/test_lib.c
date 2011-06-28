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
	char *message;
	char *failed_pos;
	char *description;
	char *error_message;

	git_testfunc function;
	unsigned failed:1, ran:1;
	jmp_buf *jump;
};

struct git_testsuite {
	char *name;
	int count, fail_count;
	git_test *list[GIT_MAX_TEST_CASES];
};

static void test_free(git_test *t)
{
	if (t) {
		free(t->name);
		free(t->description);
		free(t->failed_pos);
		free(t->message);
		free(t->error_message);
		free(t);
	}
}

static void test_run(git_test *tc)
{
	jmp_buf buf;
	tc->jump = &buf;

	if (setjmp(buf) == 0) {
		tc->ran = 1;
		(tc->function)(tc);
	}

	tc->jump = 0;
}

static git_test *create_test(git_testfunc function)
{
	git_test *t = DO_ALLOC(git_test);

	memset(t, 0x0, sizeof(git_test));
	t->function = function;

	return t;
}

void git_test__init(git_test *t, const char *name, const char *description)
{
	t->name = strdup(name);
	t->description = strdup(description);
}


/*-------------------------------------------------------------------------*
 * Public assert methods
 *-------------------------------------------------------------------------*/

static void fail_test(git_test *tc, const char *file, int line, const char *message)
{
	char buf[1024];
	const char *last_error = git_lasterror();

	snprintf(buf, 1024, "%s:%d", file, line);

	tc->failed = 1;
	tc->message = strdup(message);
	tc->failed_pos = strdup(buf);

	if (last_error)
		tc->error_message = strdup(last_error);

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

void git_test__assert_pass(git_test *tc, const char *file, int line, const char *message, int ret_value)
{
	if (ret_value < 0)
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

static void free_suite(git_testsuite *ts)
{
	unsigned int n;

	for (n = 0; n < GIT_MAX_TEST_CASES; n++)
		if (ts->list[n])
			test_free(ts->list[n]);

	free(ts->name);
	free(ts);
}

void git_testsuite_add(git_testsuite *ts, git_testfunc test)
{
	assert(ts->count < GIT_MAX_TEST_CASES);
	ts->list[ts->count++] = create_test(test);
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
				printf("  %d) \"%s\" [test %s @ %s]\n\t%s\n",
					failCount, tc->description, tc->name, tc->failed_pos, tc->message);
				if (tc->error_message)
					printf("\tError: %s\n", tc->error_message);
			}
		}
	}
}

int git_testsuite_run(git_testsuite *ts)
{
	int i, fail_count;

	printf("Suite \"%s\": ", ts->name);

	for (i = 0 ; i < ts->count ; ++i) {
		git_test *tc = ts->list[i];

		test_run(tc);
		if (tc->failed) {
			ts->fail_count++;
			putchar('F');
		} else
			putchar('.');

		fflush(stdout);
	}
	printf("\n  ");
	print_details(ts);
	fail_count = ts->fail_count;

	free_suite(ts);
	return fail_count;
}

