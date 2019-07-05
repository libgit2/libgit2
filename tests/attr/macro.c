/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "clar_libgit2.h"
#include "attr.h"

static git_repository *g_repo = NULL;

void test_attr_macro__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

void test_attr_macro__macros(void)
{
	const char *names[5] = { "rootattr", "binary", "diff", "crlf", "frotz" };
	const char *names2[5] = { "mymacro", "positive", "negative", "rootattr", "another" };
	const char *names3[3] = { "macro2", "multi2", "multi3" };
	const char *values[5];

	g_repo = cl_git_sandbox_init("attr");

	cl_git_pass(git_attr_get_many(values, g_repo, 0, "binfile", 5, names));

	cl_assert(GIT_ATTR_IS_TRUE(values[0]));
	cl_assert(GIT_ATTR_IS_TRUE(values[1]));
	cl_assert(GIT_ATTR_IS_FALSE(values[2]));
	cl_assert(GIT_ATTR_IS_FALSE(values[3]));
	cl_assert(GIT_ATTR_IS_UNSPECIFIED(values[4]));

	cl_git_pass(git_attr_get_many(values, g_repo, 0, "macro_test", 5, names2));

	cl_assert(GIT_ATTR_IS_TRUE(values[0]));
	cl_assert(GIT_ATTR_IS_TRUE(values[1]));
	cl_assert(GIT_ATTR_IS_FALSE(values[2]));
	cl_assert(GIT_ATTR_IS_UNSPECIFIED(values[3]));
	cl_assert_equal_s("77", values[4]);

	cl_git_pass(git_attr_get_many(values, g_repo, 0, "macro_test", 3, names3));

	cl_assert(GIT_ATTR_IS_TRUE(values[0]));
	cl_assert(GIT_ATTR_IS_FALSE(values[1]));
	cl_assert_equal_s("answer", values[2]);
}

void test_attr_macro__bad_macros(void)
{
	const char *names[6] = { "rootattr", "positive", "negative",
		"firstmacro", "secondmacro", "thirdmacro" };
	const char *values[6];

	g_repo = cl_git_sandbox_init("attr");

	cl_git_pass(git_attr_get_many(values, g_repo, 0, "macro_bad", 6, names));

	/* these three just confirm that the "mymacro" rule ran */
	cl_assert(GIT_ATTR_IS_UNSPECIFIED(values[0]));
	cl_assert(GIT_ATTR_IS_TRUE(values[1]));
	cl_assert(GIT_ATTR_IS_FALSE(values[2]));

	/* file contains:
	 *     # let's try some malicious macro defs
	 *     [attr]firstmacro -thirdmacro -secondmacro
	 *     [attr]secondmacro firstmacro -firstmacro
	 *     [attr]thirdmacro secondmacro=hahaha -firstmacro
	 *     macro_bad firstmacro secondmacro thirdmacro
	 *
	 * firstmacro assignment list ends up with:
	 *     -thirdmacro -secondmacro
	 * secondmacro assignment list expands "firstmacro" and ends up with:
	 *     -thirdmacro -secondmacro -firstmacro
	 * thirdmacro assignment don't expand so list ends up with:
	 *     secondmacro="hahaha"
	 *
	 * macro_bad assignment list ends up with:
	 *     -thirdmacro -secondmacro firstmacro &&
	 *     -thirdmacro -secondmacro -firstmacro secondmacro &&
	 *     secondmacro="hahaha" thirdmacro
	 *
	 * so summary results should be:
	 *     -firstmacro secondmacro="hahaha" thirdmacro
	 */
	cl_assert(GIT_ATTR_IS_FALSE(values[3]));
	cl_assert_equal_s("hahaha", values[4]);
	cl_assert(GIT_ATTR_IS_TRUE(values[5]));
}
