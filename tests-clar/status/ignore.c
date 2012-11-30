#include "clar_libgit2.h"
#include "fileops.h"
#include "git2/attr.h"
#include "ignore.h"
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
		/* pattern "ign" from .gitignore */
		{ "file", 0 },
		{ "ign", 1 },
		{ "sub", 0 },
		{ "sub/file", 0 },
		{ "sub/ign", 1 },
		{ "sub/ign/file", 1 },
		{ "sub/ign/sub", 1 },
		{ "sub/ign/sub/file", 1 },
		{ "sub/sub", 0 },
		{ "sub/sub/file", 0 },
		{ "sub/sub/ign", 1 },
		{ "sub/sub/sub", 0 },
		/* pattern "dir/" from .gitignore */
		{ "dir", 1 },
		{ "dir/", 1 },
		{ "sub/dir", 1 },
		{ "sub/dir/", 1 },
		{ "sub/dir/file", 1 }, /* contained in ignored parent */
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

void test_status_ignore__ignore_pattern_contains_space(void)
{
	unsigned int flags;
	const mode_t mode = 0777;

	g_repo = cl_git_sandbox_init("empty_standard_repo");
	cl_git_rewritefile("empty_standard_repo/.gitignore", "foo bar.txt\n");

	cl_git_mkfile(
		"empty_standard_repo/foo bar.txt", "I'm going to be ignored!");

	cl_git_pass(git_status_file(&flags, g_repo, "foo bar.txt"));
	cl_assert(flags == GIT_STATUS_IGNORED);

	cl_git_pass(git_futils_mkdir_r("empty_standard_repo/foo", NULL, mode));
	cl_git_mkfile("empty_standard_repo/foo/look-ma.txt", "I'm not going to be ignored!");

	cl_git_pass(git_status_file(&flags, g_repo, "foo/look-ma.txt"));
	cl_assert(flags == GIT_STATUS_WT_NEW);
}

void test_status_ignore__ignore_pattern_ignorecase(void)
{
	unsigned int flags;
	bool ignore_case;
	git_index *index;

	g_repo = cl_git_sandbox_init("empty_standard_repo");
	cl_git_rewritefile("empty_standard_repo/.gitignore", "a.txt\n");

	cl_git_mkfile("empty_standard_repo/A.txt", "Differs in case");

	cl_git_pass(git_repository_index(&index, g_repo));
	ignore_case = index->ignore_case;
	git_index_free(index);

	cl_git_pass(git_status_file(&flags, g_repo, "A.txt"));
	cl_assert(flags == ignore_case ? GIT_STATUS_IGNORED : GIT_STATUS_WT_NEW);
}

void test_status_ignore__subdirectories(void)
{
	status_entry_single st;
	int ignored;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_mkfile(
		"empty_standard_repo/ignore_me", "I'm going to be ignored!");

	cl_git_rewritefile("empty_standard_repo/.gitignore", "ignore_me\n");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert_equal_i(2, st.count);
	cl_assert(st.status == GIT_STATUS_IGNORED);

	cl_git_pass(git_status_file(&st.status, g_repo, "ignore_me"));
	cl_assert(st.status == GIT_STATUS_IGNORED);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "ignore_me"));
	cl_assert(ignored);


	/* So, interestingly, as per the comment in diff_from_iterators() the
	 * following file is ignored, but in a way so that it does not show up
	 * in status even if INCLUDE_IGNORED is used.  This actually matches
	 * core git's behavior - if you follow these steps and try running "git
	 * status -uall --ignored" then the following file and directory will
	 * not show up in the output at all.
	 */

	cl_git_pass(
		git_futils_mkdir_r("empty_standard_repo/test/ignore_me", NULL, 0775));
	cl_git_mkfile(
		"empty_standard_repo/test/ignore_me/file", "I'm going to be ignored!");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert_equal_i(2, st.count);

	cl_git_pass(git_status_file(&st.status, g_repo, "test/ignore_me/file"));
	cl_assert(st.status == GIT_STATUS_IGNORED);

	cl_git_pass(
		git_status_should_ignore(&ignored, g_repo, "test/ignore_me/file"));
	cl_assert(ignored);
}

void test_status_ignore__adding_internal_ignores(void)
{
	int ignored;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "one.txt"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "two.bar"));
	cl_assert(!ignored);

	cl_git_pass(git_ignore_add_rule(g_repo, "*.nomatch\n"));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "one.txt"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "two.bar"));
	cl_assert(!ignored);

	cl_git_pass(git_ignore_add_rule(g_repo, "*.txt\n"));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "one.txt"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "two.bar"));
	cl_assert(!ignored);

	cl_git_pass(git_ignore_add_rule(g_repo, "*.bar\n"));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "one.txt"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "two.bar"));
	cl_assert(ignored);

	cl_git_pass(git_ignore_clear_internal_rules(g_repo));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "one.txt"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "two.bar"));
	cl_assert(!ignored);

	cl_git_pass(git_ignore_add_rule(
		g_repo, "multiple\n*.rules\n# comment line\n*.bar\n"));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "one.txt"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "two.bar"));
	cl_assert(ignored);
}

void test_status_ignore__add_internal_as_first_thing(void)
{
	int ignored;
	const char *add_me = "\n#################\n## Eclipse\n#################\n\n*.pydevproject\n.project\n.metadata\nbin/\ntmp/\n*.tmp\n\n";

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_ignore_add_rule(g_repo, add_me));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "one.tmp"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "two.bar"));
	cl_assert(!ignored);
}

void test_status_ignore__internal_ignores_inside_deep_paths(void)
{
	int ignored;
	const char *add_me = "Debug\nthis/is/deep\npatterned*/dir\n";

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_ignore_add_rule(g_repo, add_me));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "Debug"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "and/Debug"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "really/Debug/this/file"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "Debug/what/I/say"));
	cl_assert(ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "and/NoDebug"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "NoDebug/this"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "please/NoDebug/this"));
	cl_assert(!ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "this/is/deep"));
	cl_assert(ignored);
	/* pattern containing slash gets FNM_PATHNAME so all slashes must match */
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "and/this/is/deep"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "this/is/deep/too"));
	cl_assert(ignored);
	/* pattern containing slash gets FNM_PATHNAME so all slashes must match */
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "but/this/is/deep/and/ignored"));
	cl_assert(!ignored);

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "this/is/not/deep"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "is/this/not/as/deep"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "this/is/deepish"));
	cl_assert(!ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "xthis/is/deep"));
	cl_assert(!ignored);
}

void test_status_ignore__automatically_ignore_bad_files(void)
{
	int ignored;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, ".git"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "this/file/."));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "path/../funky"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "path/whatever.c"));
	cl_assert(!ignored);

	cl_git_pass(git_ignore_add_rule(g_repo, "*.c\n"));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, ".git"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "this/file/."));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "path/../funky"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "path/whatever.c"));
	cl_assert(ignored);

	cl_git_pass(git_ignore_clear_internal_rules(g_repo));

	cl_git_pass(git_status_should_ignore(&ignored, g_repo, ".git"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "this/file/."));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "path/../funky"));
	cl_assert(ignored);
	cl_git_pass(git_status_should_ignore(&ignored, g_repo, "path/whatever.c"));
	cl_assert(!ignored);
}
