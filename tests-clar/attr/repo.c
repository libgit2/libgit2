#include "clar_libgit2.h"
#include "fileops.h"
#include "git2/attr.h"
#include "attr.h"

#include "attr_expect.h"

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
	struct attr_expected test_cases[] = {
		{ "root_test1", "repoattr", EXPECT_TRUE, NULL },
		{ "root_test1", "rootattr", EXPECT_TRUE, NULL },
		{ "root_test1", "missingattr", EXPECT_UNDEFINED, NULL },
		{ "root_test1", "subattr", EXPECT_UNDEFINED, NULL },
		{ "root_test1", "negattr", EXPECT_UNDEFINED, NULL },
		{ "root_test2", "repoattr", EXPECT_TRUE, NULL },
		{ "root_test2", "rootattr", EXPECT_FALSE, NULL },
		{ "root_test2", "missingattr", EXPECT_UNDEFINED, NULL },
		{ "root_test2", "multiattr", EXPECT_FALSE, NULL },
		{ "root_test3", "repoattr", EXPECT_TRUE, NULL },
		{ "root_test3", "rootattr", EXPECT_UNDEFINED, NULL },
		{ "root_test3", "multiattr", EXPECT_STRING, "3" },
		{ "root_test3", "multi2", EXPECT_UNDEFINED, NULL },
		{ "sub/subdir_test1", "repoattr", EXPECT_TRUE, NULL },
		{ "sub/subdir_test1", "rootattr", EXPECT_TRUE, NULL },
		{ "sub/subdir_test1", "missingattr", EXPECT_UNDEFINED, NULL },
		{ "sub/subdir_test1", "subattr", EXPECT_STRING, "yes" },
		{ "sub/subdir_test1", "negattr", EXPECT_FALSE, NULL },
		{ "sub/subdir_test1", "another", EXPECT_UNDEFINED, NULL },
		{ "sub/subdir_test2.txt", "repoattr", EXPECT_TRUE, NULL },
		{ "sub/subdir_test2.txt", "rootattr", EXPECT_TRUE, NULL },
		{ "sub/subdir_test2.txt", "missingattr", EXPECT_UNDEFINED, NULL },
		{ "sub/subdir_test2.txt", "subattr", EXPECT_STRING, "yes" },
		{ "sub/subdir_test2.txt", "negattr", EXPECT_FALSE, NULL },
		{ "sub/subdir_test2.txt", "another", EXPECT_STRING, "zero" },
		{ "sub/subdir_test2.txt", "reposub", EXPECT_TRUE, NULL },
		{ "sub/sub/subdir.txt", "another", EXPECT_STRING, "one" },
		{ "sub/sub/subdir.txt", "reposubsub", EXPECT_TRUE, NULL },
		{ "sub/sub/subdir.txt", "reposub", EXPECT_UNDEFINED, NULL },
		{ "does-not-exist", "foo", EXPECT_STRING, "yes" },
		{ "sub/deep/file", "deepdeep", EXPECT_TRUE, NULL },
		{ "sub/sub/d/no", "test", EXPECT_STRING, "a/b/d/*" },
		{ "sub/sub/d/yes", "test", EXPECT_UNDEFINED, NULL },
		{ NULL, NULL, 0, NULL }
	}, *scan;

	for (scan = test_cases; scan->path != NULL; scan++) {
		const char *value;
		cl_git_pass(git_attr_get(g_repo, scan->path, scan->attr, &value));
		attr_check_expected(scan->expected, scan->expected_str, value);
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

	cl_assert(GIT_ATTR_TRUE(values[0]));
	cl_assert(GIT_ATTR_TRUE(values[1]));
	cl_assert(GIT_ATTR_UNSPECIFIED(values[2]));
	cl_assert(GIT_ATTR_UNSPECIFIED(values[3]));

	cl_git_pass(git_attr_get_many(g_repo, "root_test2", 4, names, values));

	cl_assert(GIT_ATTR_TRUE(values[0]));
	cl_assert(GIT_ATTR_FALSE(values[1]));
	cl_assert(GIT_ATTR_UNSPECIFIED(values[2]));
	cl_assert(GIT_ATTR_UNSPECIFIED(values[3]));

	cl_git_pass(git_attr_get_many(g_repo, "sub/subdir_test1", 4, names, values));

	cl_assert(GIT_ATTR_TRUE(values[0]));
	cl_assert(GIT_ATTR_TRUE(values[1]));
	cl_assert(GIT_ATTR_UNSPECIFIED(values[2]));
	cl_assert_strequal("yes", values[3]);
}

static int count_attrs(
	const char *name,
	const char *value,
	void *payload)
{
	GIT_UNUSED(name);
	GIT_UNUSED(value);

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
	cl_assert(GIT_ATTR_TRUE(value));

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "bar", &value));
	cl_assert(GIT_ATTR_UNSPECIFIED(value));

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "baz", &value));
	cl_assert(GIT_ATTR_FALSE(value));

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "merge", &value));
	cl_assert_strequal("filfre", value);

	cl_git_pass(git_attr_get(g_repo, "sub/abc", "frotz", &value));
	cl_assert(GIT_ATTR_UNSPECIFIED(value));
}

void test_attr_repo__macros(void)
{
	const char *names[5] = { "rootattr", "binary", "diff", "crlf", "frotz" };
	const char *names2[5] = { "mymacro", "positive", "negative", "rootattr", "another" };
	const char *names3[3] = { "macro2", "multi2", "multi3" };
	const char *values[5];

	cl_git_pass(git_attr_get_many(g_repo, "binfile", 5, names, values));

	cl_assert(GIT_ATTR_TRUE(values[0]));
	cl_assert(GIT_ATTR_TRUE(values[1]));
	cl_assert(GIT_ATTR_FALSE(values[2]));
	cl_assert(GIT_ATTR_FALSE(values[3]));
	cl_assert(GIT_ATTR_UNSPECIFIED(values[4]));

	cl_git_pass(git_attr_get_many(g_repo, "macro_test", 5, names2, values));

	cl_assert(GIT_ATTR_TRUE(values[0]));
	cl_assert(GIT_ATTR_TRUE(values[1]));
	cl_assert(GIT_ATTR_FALSE(values[2]));
	cl_assert(GIT_ATTR_UNSPECIFIED(values[3]));
	cl_assert_strequal("77", values[4]);

	cl_git_pass(git_attr_get_many(g_repo, "macro_test", 3, names3, values));

	cl_assert(GIT_ATTR_TRUE(values[0]));
	cl_assert(GIT_ATTR_FALSE(values[1]));
	cl_assert_strequal("answer", values[2]);
}

void test_attr_repo__bad_macros(void)
{
	const char *names[6] = { "rootattr", "positive", "negative",
		"firstmacro", "secondmacro", "thirdmacro" };
	const char *values[6];

	cl_git_pass(git_attr_get_many(g_repo, "macro_bad", 6, names, values));

	/* these three just confirm that the "mymacro" rule ran */
	cl_assert(GIT_ATTR_UNSPECIFIED(values[0]));
	cl_assert(GIT_ATTR_TRUE(values[1]));
	cl_assert(GIT_ATTR_FALSE(values[2]));

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
	cl_assert(GIT_ATTR_FALSE(values[3]));
	cl_assert_strequal("hahaha", values[4]);
	cl_assert(GIT_ATTR_TRUE(values[5]));
}

