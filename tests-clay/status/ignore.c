#include "clay_libgit2.h"
#include "fileops.h"
#include "git2/attr.h"

static git_repository *g_repo = NULL;

void test_status_ignore__initialize(void)
{
	/* Before each test, instantiate the attr repo from the fixtures and
	 * rename the .gitted to .git so it is a repo with a working dir.  Also
	 * rename gitignore to .gitignore.
	 */
	cl_fixture_sandbox("attr");
	cl_git_pass(p_rename("attr/.gitted", "attr/.git"));
	cl_git_pass(p_rename("attr/gitignore", "attr/.gitignore"));
	cl_git_pass(git_repository_open(&g_repo, "attr/.git"));
}

void test_status_ignore__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup("attr");
}

void test_status_ignore__0(void)
{
	struct {
		const char *path;
		int expected;
	} test_cases[] = {
		{ "file", 0 },
		{ "ign", 1 },
		{ "sub", 1 },
		{ "sub/file", 0 },
		{ "sub/ign", 1 },
		{ "sub/sub", 1 },
		{ "sub/sub/file", 0 },
		{ "sub/sub/ign", 1 },
		{ "sub/sub/sub", 1 },
		{ NULL, 0 }
	}, *one_test;

	for (one_test = test_cases; one_test->path != NULL; one_test++) {
		int ignored;
		cl_git_pass(git_status_should_ignore(g_repo, one_test->path, &ignored));
		cl_assert_(ignored == one_test->expected, one_test->path);
	}
}
