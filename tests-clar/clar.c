/*
 * Copyright (c) Vicent Marti. All rights reserved.
 *
 * This file is part of clar, distributed under the ISC license.
 * For full terms see the included COPYING file.
 */
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

/* required for sandboxing */
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#	include <windows.h>
#	include <io.h>
#	include <shellapi.h>
#	include <direct.h>

#	define _MAIN_CC __cdecl

#	define stat(path, st) _stat(path, st)
#	define mkdir(path, mode) _mkdir(path)
#	define chdir(path) _chdir(path)
#	define access(path, mode) _access(path, mode)
#	define strdup(str) _strdup(str)
#	define strcasecmp(a,b) _stricmp(a,b)

#	ifndef __MINGW32__
#		pragma comment(lib, "shell32")
#		define strncpy(to, from, to_size) strncpy_s(to, to_size, from, _TRUNCATE)
#		define W_OK 02
#		define S_ISDIR(x) ((x & _S_IFDIR) != 0)
#		define snprint_eq(buf,sz,fmt,a,b) _snprintf_s(buf,sz,_TRUNCATE,fmt,a,b)
#	else
#		define snprint_eq snprintf
#	endif
	typedef struct _stat STAT_T;
#else
#	include <sys/wait.h> /* waitpid(2) */
#	include <unistd.h>
#	define _MAIN_CC
#	define snprint_eq snprintf
	typedef struct stat STAT_T;
#endif

#include "clar.h"

static void fs_rm(const char *_source);
static void fs_copy(const char *_source, const char *dest);

static const char *
fixture_path(const char *base, const char *fixture_name);

struct clar_error {
	const char *test;
	int test_number;
	const char *suite;
	const char *file;
	int line_number;
	const char *error_msg;
	char *description;

	struct clar_error *next;
};

static struct {
	const char *active_test;
	const char *active_suite;

	int suite_errors;
	int total_errors;

	int tests_ran;
	int suites_ran;

	int report_errors_only;
	int exit_on_error;
	int report_suite_names;

	struct clar_error *errors;
	struct clar_error *last_error;

	void (*local_cleanup)(void *);
	void *local_cleanup_payload;

	jmp_buf trampoline;
	int trampoline_enabled;
} _clar;

struct clar_func {
	const char *name;
	void (*ptr)(void);
};

struct clar_suite {
	const char *name;
	struct clar_func initialize;
	struct clar_func cleanup;
	const struct clar_func *tests;
	size_t test_count;
	int enabled;
};

/* From clar_print_*.c */
static void clar_print_init(int test_count, int suite_count, const char *suite_names);
static void clar_print_shutdown(int test_count, int suite_count, int error_count);
static void clar_print_error(int num, const struct clar_error *error);
static void clar_print_ontest(const char *test_name, int test_number, int failed);
static void clar_print_onsuite(const char *suite_name, int suite_index);
static void clar_print_onabort(const char *msg, ...);

/* From clar_sandbox.c */
static void clar_unsandbox(void);
static int clar_sandbox(void);

/* Load the declarations for the test suite */
#include "clar.suite"

/* Core test functions */
static void
clar_report_errors(void)
{
	int i = 1;
	struct clar_error *error, *next;

	error = _clar.errors;
	while (error != NULL) {
		next = error->next;
		clar_print_error(i++, error);
		free(error->description);
		free(error);
		error = next;
	}

	_clar.errors = _clar.last_error = NULL;
}

static void
clar_run_test(
	const struct clar_func *test,
	const struct clar_func *initialize,
	const struct clar_func *cleanup)
{
	int error_st = _clar.suite_errors;

	_clar.trampoline_enabled = 1;

	if (setjmp(_clar.trampoline) == 0) {
		if (initialize->ptr != NULL)
			initialize->ptr();

		test->ptr();
	}

	_clar.trampoline_enabled = 0;

	if (_clar.local_cleanup != NULL)
		_clar.local_cleanup(_clar.local_cleanup_payload);

	if (cleanup->ptr != NULL)
		cleanup->ptr();

	_clar.tests_ran++;

	/* remove any local-set cleanup methods */
	_clar.local_cleanup = NULL;
	_clar.local_cleanup_payload = NULL;

	if (_clar.report_errors_only)
		clar_report_errors();
	else
		clar_print_ontest(
			test->name,
			_clar.tests_ran,
			(_clar.suite_errors > error_st)
		);
}

static void
clar_run_suite(const struct clar_suite *suite)
{
	const struct clar_func *test = suite->tests;
	size_t i;

	if (!suite->enabled)
		return;

	if (_clar.exit_on_error && _clar.total_errors)
		return;

	if (!_clar.report_errors_only)
		clar_print_onsuite(suite->name, ++_clar.suites_ran);

	_clar.active_suite = suite->name;
	_clar.suite_errors = 0;

	for (i = 0; i < suite->test_count; ++i) {
		_clar.active_test = test[i].name;
		clar_run_test(&test[i], &suite->initialize, &suite->cleanup);

		if (_clar.exit_on_error && _clar.total_errors)
			return;
	}
}

static void
clar_usage(const char *arg)
{
	printf("Usage: %s [options]\n\n", arg);
	printf("Options:\n");
	printf("  -sname\tRun only the suite with `name`\n");
	printf("  -iname\tInclude the suite with `name`\n");
	printf("  -xname\tExclude the suite with `name`\n");
	printf("  -q    \tOnly report tests that had an error\n");
	printf("  -Q    \tQuit as soon as a test fails\n");
	printf("  -l    \tPrint suite names\n");
	exit(-1);
}

static void
clar_parse_args(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; ++i) {
		char *argument = argv[i];

		if (argument[0] != '-')
			clar_usage(argv[0]);

		switch (argument[1]) {
		case 's':
		case 'i':
		case 'x': { /* given suite name */
			int offset = (argument[2] == '=') ? 3 : 2, found = 0;
			char action = argument[1];
			size_t j, len;

			argument += offset;
			len = strlen(argument);

			if (len == 0)
				clar_usage(argv[0]);

			for (j = 0; j < _clar_suite_count; ++j) {
				if (strncmp(argument, _clar_suites[j].name, len) == 0) {
					int exact = !strcmp(argument, _clar_suites[j].name);

					++found;

					if (!exact)
						_clar.report_suite_names = 1;

					switch (action) {
						case 's': clar_run_suite(&_clar_suites[j]); break;
						case 'i': _clar_suites[j].enabled = 1; break;
						case 'x': _clar_suites[j].enabled = 0; break;
					}

					if (exact)
						break;
				}
			}

			if (!found) {
				clar_print_onabort("No suite matching '%s' found.\n", argument);
				exit(-1);
			}
			break;
		}

		case 'q':
			_clar.report_errors_only = 1;
			break;

		case 'Q':
			_clar.exit_on_error = 1;
			break;

		case 'l': {
			size_t j;
			printf("Test suites (use -s<name> to run just one):\n");
			for (j = 0; j < _clar_suite_count; ++j)
				printf(" %3d: %s\n", (int)j, _clar_suites[j].name);

			exit(0);
		}

		default:
			clar_usage(argv[0]);
		}
	}
}

int
clar_test(int argc, char **argv)
{
	clar_print_init(
		(int)_clar_callback_count,
		(int)_clar_suite_count,
		""
	);

	if (clar_sandbox() < 0) {
		clar_print_onabort("Failed to sandbox the test runner.\n");
		exit(-1);
	}

	if (argc > 1)
		clar_parse_args(argc, argv);

	if (!_clar.suites_ran) {
		size_t i;
		for (i = 0; i < _clar_suite_count; ++i)
			clar_run_suite(&_clar_suites[i]);
	}

	clar_print_shutdown(
		_clar.tests_ran,
		(int)_clar_suite_count,
		_clar.total_errors
	);

	clar_unsandbox();
	return _clar.total_errors;
}

void
clar__assert(
	int condition,
	const char *file,
	int line,
	const char *error_msg,
	const char *description,
	int should_abort)
{
	struct clar_error *error;

	if (condition)
		return;

	error = calloc(1, sizeof(struct clar_error));

	if (_clar.errors == NULL)
		_clar.errors = error;

	if (_clar.last_error != NULL)
		_clar.last_error->next = error;

	_clar.last_error = error;

	error->test = _clar.active_test;
	error->test_number = _clar.tests_ran;
	error->suite = _clar.active_suite;
	error->file = file;
	error->line_number = line;
	error->error_msg = error_msg;

	if (description != NULL)
		error->description = strdup(description);

	_clar.suite_errors++;
	_clar.total_errors++;

	if (should_abort) {
		if (!_clar.trampoline_enabled) {
			clar_print_onabort(
				"Fatal error: a cleanup method raised an exception.");
			clar_report_errors();
			exit(-1);
		}

		longjmp(_clar.trampoline, -1);
	}
}

void clar__assert_equal_s(
	const char *s1,
	const char *s2,
	const char *file,
	int line,
	const char *err,
	int should_abort)
{
	int match = (s1 == NULL || s2 == NULL) ? (s1 == s2) : (strcmp(s1, s2) == 0);

	if (!match) {
		char buf[4096];
		snprint_eq(buf, 4096, "'%s' != '%s'", s1, s2);
		clar__assert(0, file, line, err, buf, should_abort);
	}
}

void clar__assert_equal_i(
	int i1,
	int i2,
	const char *file,
	int line,
	const char *err,
	int should_abort)
{
	if (i1 != i2) {
		char buf[128];
		snprint_eq(buf, 128, "%d != %d", i1, i2);
		clar__assert(0, file, line, err, buf, should_abort);
	}
}

void cl_set_cleanup(void (*cleanup)(void *), void *opaque)
{
	_clar.local_cleanup = cleanup;
	_clar.local_cleanup_payload = opaque;
}

#include "clar/sandbox.h"
#include "clar/fixtures.h"
#include "clar/fs.h"
#include "clar/print.h"
