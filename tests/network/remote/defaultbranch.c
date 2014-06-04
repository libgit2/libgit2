#include "clar_libgit2.h"
#include "buffer.h"
#include "refspec.h"
#include "remote.h"

static git_remote *g_remote;
static git_repository *g_repo_a, *g_repo_b;

void test_network_remote_defaultbranch__initialize(void)
{
	g_repo_a = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_repository_init(&g_repo_b, "repo-b.git", true));
	cl_git_pass(git_remote_create(&g_remote, g_repo_b, "origin", git_repository_path(g_repo_a)));
}

void test_network_remote_defaultbranch__cleanup(void)
{
	git_remote_free(g_remote);
	git_repository_free(g_repo_b);

	cl_git_sandbox_cleanup();
	cl_fixture_cleanup("repo-b.git");
}

static void assert_default_branch(const char *should)
{
	git_buf name = GIT_BUF_INIT;

	cl_git_pass(git_remote_connect(g_remote, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_default_branch(&name, g_remote));
	cl_assert_equal_s(should, name.ptr);
	git_buf_free(&name);
}

void test_network_remote_defaultbranch__master(void)
{
	assert_default_branch("refs/heads/master");
}

void test_network_remote_defaultbranch__master_does_not_win(void)
{
	cl_git_pass(git_repository_set_head(g_repo_a, "refs/heads/not-good", NULL, NULL));
	assert_default_branch("refs/heads/not-good");
}

void test_network_remote_defaultbranch__master_on_detached(void)
{
	cl_git_pass(git_repository_detach_head(g_repo_a, NULL, NULL));
	assert_default_branch("refs/heads/master");
}
