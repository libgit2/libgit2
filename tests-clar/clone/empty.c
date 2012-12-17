#include "clar_libgit2.h"

#include "git2/clone.h"
#include "repository.h"

static git_clone_options g_options;
static git_remote *g_origin;
static git_repository *g_repo;

void test_clone_empty__initialize(void)
{
	git_repository *sandbox = cl_git_sandbox_init("empty_bare.git");
	cl_git_remove_placeholders(git_repository_path(sandbox), "dummy-marker.txt");

	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
	cl_git_pass(git_remote_new(&g_origin, NULL, "origin", cl_git_fixture_url("testrepo.git"), GIT_REMOTE_DEFAULT_FETCH));
}

void test_clone_empty__cleanup(void)
{
	git_remote_free(g_origin);
	g_origin = NULL;
	cl_git_sandbox_cleanup();
}

static void cleanup_repository(void *path)
{
	cl_fixture_cleanup((const char *)path);
}

void test_clone_empty__can_clone_an_empty_local_repo_barely(void)
{
	cl_set_cleanup(&cleanup_repository, "./empty");

	git_remote_free(g_origin);
	g_origin = NULL;
	cl_git_pass(git_remote_new(&g_origin, NULL, "origin", "./empty_bare.git", GIT_REMOTE_DEFAULT_FETCH));

	g_options.bare = true;
	cl_git_pass(git_clone(&g_repo, g_origin, "./empty", &g_options));
}

void test_clone_empty__can_clone_an_empty_local_repo(void)
{
	cl_set_cleanup(&cleanup_repository, "./empty");

	git_remote_free(g_origin);
	g_origin = NULL;
	cl_git_pass(git_remote_new(&g_origin, NULL, "origin", "./empty_bare.git", GIT_REMOTE_DEFAULT_FETCH));

	cl_git_pass(git_clone(&g_repo, g_origin, "./empty", &g_options));
}

void test_clone_empty__can_clone_an_empty_standard_repo(void)
{
	cl_git_sandbox_cleanup();
	g_repo = cl_git_sandbox_init("empty_standard_repo");
	cl_git_remove_placeholders(git_repository_path(g_repo), "dummy-marker.txt");

	git_remote_free(g_origin);
	g_origin = NULL;
	cl_git_pass(git_remote_new(&g_origin, NULL, "origin", "./empty_standard_repo", GIT_REMOTE_DEFAULT_FETCH));

	cl_set_cleanup(&cleanup_repository, "./empty");

	cl_git_pass(git_clone(&g_repo, g_origin, "./empty", &g_options));
}
