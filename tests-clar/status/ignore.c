#include "clar_libgit2.h"
#include "fileops.h"
#include "git2/attr.h"
#include "attr.h"

static git_repository *g_repo = NULL;

void test_status_ignore__initialize(void)
{
	g_repo = cl_git_sandbox_init("attr");
}

void test_status_ignore__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_status_ignore__0(void)
{
	struct {
		const char *path;
		int expected;
	} test_cases[] = {
		/* patterns "sub" and "ign" from .gitignore */
		{ "file", 0 },
		{ "ign", 1 },
		{ "sub", 1 },
		{ "sub/file", 0 },
		{ "sub/ign", 1 },
		{ "sub/sub", 1 },
		{ "sub/sub/file", 0 },
		{ "sub/sub/ign", 1 },
		{ "sub/sub/sub", 1 },
		/* pattern "dir/" from .gitignore */
		{ "dir", 1 },
		{ "dir/", 1 },
		{ "sub/dir", 1 },
		{ "sub/dir/", 1 },
		{ "sub/sub/dir", 0 }, /* dir is not actually a dir, but a file */
		{ NULL, 0 }
	}, *one_test;

	for (one_test = test_cases; one_test->path != NULL; one_test++) {
		int ignored;
		cl_git_pass(git_status_should_ignore(g_repo, one_test->path, &ignored));
		cl_assert_(ignored == one_test->expected, one_test->path);
	}

	/* confirm that ignore files were cached */
	cl_git_pass(git_attr_cache__is_cached(g_repo, ".git/info/exclude"));
	cl_git_pass(git_attr_cache__is_cached(g_repo, ".gitignore"));
}
