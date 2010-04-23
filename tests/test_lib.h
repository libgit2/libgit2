/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include <git/common.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/** Declare a function never returns to the caller. */
#ifdef __GNUC__
# define NORETURN __attribute__((__noreturn__))
#elif defined(_MSC_VER)
# define NORETURN __declspec(noreturn)
#else
# define NORETURN /* noreturn */
#endif

/**
 * Declares a new test block starting, with the specified name.
 * @param name C symbol to assign to this test's function.
 */
#define BEGIN_TEST(name) \
	void testfunc__##name(void) \
	{ \
		test_begin(#name, __FILE__, __LINE__); \
	{

/** Denote the end of a test. */
#define END_TEST \
	} \
		test_end(); \
	}

/* These are internal functions for BEGIN_TEST, END_TEST. */
extern void test_begin(const char *, const char *, int);
extern void test_end(void);

/**
 * Abort the current test suite.
 *
 * This function terminates the current test suite
 * and does not return to the caller.
 *
 * @param fmt printf style format string.
 */
extern void NORETURN test_die(const char *fmt, ...)
	GIT_FORMAT_PRINTF(1, 2);

/**
 * Evaluate a library function which must return success.
 *
 * The definition of "success" is the classical 0 return value.
 * This macro makes the test suite fail if the expression evaluates
 * to a non-zero result.  It is suitable for testing most API
 * functions in the library.
 *
 * @param expr the expression to evaluate, and test the result of.
 */
#define must_pass(expr) \
	if (expr) test_die("line %d: %s", __LINE__, #expr)

/**
 * Evaluate a library function which must return an error.
 *
 * The definition of "failure" is the classical non-0 return value.
 * This macro makes the test suite fail if the expression evaluates
 * to 0 (aka success).  It is suitable for testing most API
 * functions in the library.
 *
 * @param expr the expression to evaluate, and test the result of.
 */
#define must_fail(expr) \
	if (!(expr)) test_die("line %d: %s", __LINE__, #expr)

/**
 * Evaluate an expression which must produce a true result.
 *
 * @param expr the expression to evaluate, and test the result of.
 */
#define must_be_true(expr) \
	if (!(expr)) test_die("line %d: %s", __LINE__, #expr)
