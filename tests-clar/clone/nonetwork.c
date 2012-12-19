#include "clar_libgit2.h"

#include "git2/clone.h"
#include "repository.h"

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"

static git_clone_options g_options;
static git_repository *g_repo;

void test_clone_nonetwork__initialize(void)
{
	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
}

static void cleanup_repository(void *path)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	cl_fixture_cleanup((const char *)path);
}

void test_clone_nonetwork__bad_url(void)
{
	/* Clone should clean up the mess if the URL isn't a git repository */
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(!git_path_exists("./foo"));
	g_options.bare = true;
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(!git_path_exists("./foo"));
}

void test_clone_nonetwork__local(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__local_absolute_path(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	const char *local_src = cl_fixture("testrepo.git");
	cl_git_pass(git_clone(&g_repo, local_src, "./foo", &g_options));
}

void test_clone_nonetwork__local_bare(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	g_options.bare = true;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_when_the_target_is_a_file(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	cl_git_mkfile("./foo", "Bar!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_with_already_existing_but_non_empty_directory(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_mkfile("./foo/bar", "Baz!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}
