#include "clar_libgit2.h"
#include "fileops.h"
#include "git2/attr.h"
#include "attr.h"

static git_repository *g_repo = NULL;

void test_attr_repo__initialize(void)
{
	/* Before each test, instantiate the attr repo from the fixtures and
	 * rename the .gitted to .git so it is a repo with a working dir.
	 * Also rename gitattributes to .gitattributes, because it contains
	 * macro definitions which are only allowed in the root.
	 */
	cl_fixture_sandbox("attr");
	cl_git_pass(p_rename("attr/.gitted", "attr/.git"));
	cl_git_pass(p_rename("attr/gitattributes", "attr/.gitattributes"));
	cl_git_pass(git_repository_open(&g_repo, "attr/.git"));
}

void test_attr_repo__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup("attr");
}

void test_attr_repo__get_one(void)
{
	const char *value;
	struct {
		const char *file;
		const char *attr;
		const char *expected;
	} test_cases[] = {
		{ "root_test1", "repoattr", GIT_ATTR_TRUE },
		{ "root_test1", "rootattr", GIT_ATTR_TRUE },
		{ "root_test1", "missingattr", NULL },
		{ "root_test1", "subattr", NULL },
		{ "root_test1", "negattr", NULL },
		{ "root_test2", "repoattr", GIT_ATTR_TRUE },
		{ "root_test2", "rootattr", GIT_ATTR_FALSE },
		{ "root_test2", "missingattr", NULL },
		{ "root_test2", "multiattr", GIT_ATTR_FALSE },
		{ "root_test3", "repoattr", GIT_ATTR_TRUE },
		{ "root_test3", "rootattr", NULL },
		{ "root_test3", "multiattr", "3" },
		{ "root_test3", "multi2", NULL },
		{ "sub/subdir_test1", "repoattr", GIT_ATTR_TRUE },
		{ "sub/subdir_test1", "rootattr", GIT_ATTR_TRUE },
		{ "sub/subdir_test1", "missingattr", NULL },
		{ "sub/subdir_test1", "subattr", "yes" },
		{ "sub/subdir_test1", "negattr", GIT_ATTR_FALSE },
		{ "sub/subdir_test1", "another", NULL },
		{ "sub/subdir_test2.txt", "repoattr", GIT_ATTR_TRUE },
		{ "sub/subdir_test2.txt", "rootattr", GIT_ATTR_TRUE },
		{ "sub/subdir_test2.txt", "missingattr", NULL },
		{ "sub/subdir_test2.txt", "subattr", "yes" },
		{ "sub/subdir_test2.txt", "negattr", GIT_ATTR_FALSE },
		{ "sub/subdir_test2.txt", "another", "zero" },
		{ "sub/subdir_test2.txt", "reposub", GIT_ATTR_TRUE },
		{ "sub/sub/subdir.txt", "another", "one" },
		{ "sub/sub/subdir.txt", "reposubsub", GIT_ATTR_TRUE },
		{ "sub/sub/subdir.txt", "reposub", NULL },
		{ "does-not-exist", "foo", "yes" },
		{ "sub/deep/file", "deepdeep", GIT_ATTR_TRUE },
		{ NULL, NULL, NULL }
	}, *scan;

	for (scan = test_cases; scan->file != NULL; scan++) {
		git_buf b = GIT_BUF_INIT;

		git_buf_printf(&b, "%s:%s == expect %s",
					   scan->file, scan->attr, scan->expected);

		cl_must_pass_(
			git_attr_get(g_repo, scan->file, scan->attr, &value) == GIT_SUCCESS,
			b.ptr);

		git_buf_printf(&b, ", got %s", value);

		if (scan->expected == NULL ||
			scan->expected == GIT_ATTR_TRUE ||
			scan->expected == GIT_ATTR_FALSE)
		{
			cl_assert_(scan->expected == value, b.ptr);
		} else {
			cl_assert_strequal(scan->expected, value);
		}

		git_buf_free(&b);
	}

	cl_git_pass(git_attr_cache__is_cached(g_repo, ".git/info/attributes"));
	cl_git_pass(git_attr_cache__is_cached(g_repo, ".gitattributes"));
	cl_git_pass(git_attr_cache__is_cached(g_repo, "sub/.gitattributes"));
}

void test_attr_repo__get_many(void)
{
	const char *names[4] = { "repoattr", "rootattr", "missingattr", "subattr" };
	const char *values[4];

	cl_git_pass(git_attr_get_many(g_repo, "root_test1", 4, names, values));

	cl_assert(values[0] == GIT_ATTR_TRUE);
	cl_assert(values[1] == GIT_ATTR_TRUE);
	cl_assert(values[2] == NULL);
	cl_assert(values[3] == NULL);

	cl_git_pass(git_attr_get_many(g_repo, "root_test2", 4, names, values));

	cl_assert(values[0] == GIT_ATTR_TRUE);
	cl_assert(values[1] == GIT_ATTR_FALSE);
	cl_assert(values[2] == NULL);
	cl_assert(values[3] == NULL);

	cl_git_pass(git_attr_get_many(g_repo, "sub/subdir_test1", 4, names, values));

	cl_assert(values[0] == GIT_ATTR_TRUE);
	cl_assert(values[1] == GIT_ATTR_TRUE);
	cl_assert(values[2] == NULL);
	cl_assert_strequal("yes", values[3]);

}

static int count_attrs(
	const char *GIT_UNUSED(name),
	const char *GIT_UNUSED(value),
	void *payload)
{
	GIT_UNUSED_ARG(name);
	GIT_UNUSED_ARG(value);

	*((int *)payload) += 1;

	return GIT_SUCCESS;
}

void test_attr_repo__foreach(void)
{
	int count;

	count = 0;
	cl_git_pass(git_attr_foreach(g_repo, "root_test1", &count_attrs, &count));
	cl_assert(count == 2);

	count = 0;
	cl_git_pass(git_attr_foreach(g_repo, "sub/subdir_test1",
		&count_attrs, &count));
	cl_assert(count == 4); /* repoattr, rootattr, subattr, negattr */

	count = 0;
	cl_git_pass(git_attr_foreach(g_repo, "sub/subdir_test2.txt",
		&count_attrs, &count));
	cl_assert(count == 6); /* repoattr, rootattr, subattr, reposub, negattr, another */
}

void test_attr_repo__manpage_example(void)
{
	const char *value;

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "foo", &value));
	cl_assert(value == GIT_ATTR_TRUE);

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "bar", &value));
	cl_assert(value == NULL);

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "baz", &value));
	cl_assert(value == GIT_ATTR_FALSE);

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "merge", &value));
	cl_assert_strequal("filfre", value);

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "frotz", &value));
	cl_assert(value == NULL);
}

void test_attr_repo__macros(void)
{
	const char *names[5] = { "rootattr", "binary", "diff", "crlf", "frotz" };
	const char *names2[5] = { "mymacro", "positive", "negative", "rootattr", "another" };
	const char *names3[3] = { "macro2", "multi2", "multi3" };
	const char *values[5];

	cl_git_pass(git_attr_get_many(g_repo, "binfile", 5, names, values));

	cl_assert(values[0] == GIT_ATTR_TRUE);
	cl_assert(values[1] == GIT_ATTR_TRUE);
	cl_assert(values[2] == GIT_ATTR_FALSE);
	cl_assert(values[3] == GIT_ATTR_FALSE);
	cl_assert(values[4] == NULL);

	cl_git_pass(git_attr_get_many(g_repo, "macro_test", 5, names2, values));

	cl_assert(values[0] == GIT_ATTR_TRUE);
	cl_assert(values[1] == GIT_ATTR_TRUE);
	cl_assert(values[2] == GIT_ATTR_FALSE);
	cl_assert(values[3] == NULL);
	cl_assert_strequal("77", values[4]);

	cl_git_pass(git_attr_get_many(g_repo, "macro_test", 3, names3, values));

	cl_assert(values[0] == GIT_ATTR_TRUE);
	cl_assert(values[1] == GIT_ATTR_FALSE);
	cl_assert_strequal("answer", values[2]);
}

void test_attr_repo__bad_macros(void)
{
	const char *names[6] = { "rootattr", "positive", "negative",
		"firstmacro", "secondmacro", "thirdmacro" };
	const char *values[6];

	cl_git_pass(git_attr_get_many(g_repo, "macro_bad", 6, names, values));

	/* these three just confirm that the "mymacro" rule ran */
	cl_assert(values[0] == NULL);
	cl_assert(values[1] == GIT_ATTR_TRUE);
	cl_assert(values[2] == GIT_ATTR_FALSE);

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
	cl_assert(values[3] == GIT_ATTR_FALSE);
	cl_assert_strequal("hahaha", values[4]);
	cl_assert(values[5] == GIT_ATTR_TRUE);
}
