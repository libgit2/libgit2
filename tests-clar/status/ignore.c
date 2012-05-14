#include "clar_libgit2.h"
#include "fileops.h"
#include "git2/attr.h"
#include "attr.h"
#include "status_helpers.h"

static git_repository *g_repo = NULL;

void test_status_ignore__initialize(void)
{
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

	g_repo = cl_git_sandbox_init("attr");

	for (one_test = test_cases; one_test->path != NULL; one_test++) {
		int ignored;
		cl_git_pass(git_status_should_ignore(&ignored, g_repo, one_test->path));
		cl_assert_(ignored == one_test->expected, one_test->path);
	}

	/* confirm that ignore files were cached */
	cl_assert(git_attr_cache__is_cached(g_repo, 0, ".git/info/exclude"));
	cl_assert(git_attr_cache__is_cached(g_repo, 0, ".gitignore"));
}


void test_status_ignore__1(void)
{
	int ignored;

	g_repo = cl_git_sandbox_init("attr");

	cl_git_rewritefile("attr/.gitignore", "/*.txt\n/dir/\n");
	git_attr_cache_flush(g_repo);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "root_test4.txt"));
	cl_assert(ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "sub/subdir_test2.txt"));
	cl_assert(!ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "dir"));
	cl_assert(ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "dir/"));
	cl_assert(ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "sub/dir"));
	cl_assert(!ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "sub/dir/"));
	cl_assert(!ignored);
}


void test_status_ignore__empty_repo_with_gitignore_rewrite(void)
{
	status_entry_single st;
	int ignored;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_mkfile(
		"empty_standard_repo/look-ma.txt", "I'm going to be ignored!");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert(st.count == 1);
	cl_assert(st.status == GIT_STATUS_WT_NEW);

	cl_git_pass(git_status_file(&st.status, g_repo, "look-ma.txt"));
	cl_assert(st.status == GIT_STATUS_WT_NEW);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "look-ma.txt"));
	cl_assert(!ignored);

	cl_git_rewritefile("empty_standard_repo/.gitignore", "*.nomatch\n");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert(st.count == 2);
	cl_assert(st.status == GIT_STATUS_WT_NEW);

	cl_git_pass(git_status_file(&st.status, g_repo, "look-ma.txt"));
	cl_assert(st.status == GIT_STATUS_WT_NEW);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "look-ma.txt"));
	cl_assert(!ignored);

	cl_git_rewritefile("empty_standard_repo/.gitignore", "*.txt\n");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert(st.count == 2);
	cl_assert(st.status == GIT_STATUS_IGNORED);

	cl_git_pass(git_status_file(&st.status, g_repo, "look-ma.txt"));
	cl_assert(st.status == GIT_STATUS_IGNORED);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "look-ma.txt"));
	cl_assert(ignored);
}

