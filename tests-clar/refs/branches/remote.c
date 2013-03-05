#include "clar_libgit2.h"
#include "branch.h"
#include "remote.h"

static git_repository *g_repo;
static const char *remote_tracking_branch_name = "refs/remotes/test/master";
static const char *expected_remote_name = "test";
static int expected_remote_name_length;

void test_refs_branches_remote__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");

	expected_remote_name_length = (int)strlen(expected_remote_name) + 1;
}

void test_refs_branches_remote__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_refs_branches_remote__can_get_remote_for_branch(void)
{
	char remotename[1024] = {0};

	cl_assert_equal_i(expected_remote_name_length,
		git_branch_remote_name(NULL, 0, g_repo, remote_tracking_branch_name));

	cl_assert_equal_i(expected_remote_name_length,
		git_branch_remote_name(remotename, expected_remote_name_length, g_repo,
			remote_tracking_branch_name));

	cl_assert_equal_s("test", remotename);
}

void test_refs_branches_remote__insufficient_buffer_returns_error(void)
{
	char remotename[1024] = {0};

	cl_assert_equal_i(expected_remote_name_length,
		git_branch_remote_name(NULL, 0, g_repo, remote_tracking_branch_name));

	cl_git_fail_with(git_branch_remote_name(remotename,
		expected_remote_name_length - 1, g_repo, remote_tracking_branch_name),
			GIT_ERROR);
}

void test_refs_branches_remote__no_matching_remote_returns_error(void)
{
	const char *unknown = "refs/remotes/nonexistent/master";

	cl_git_fail_with(git_branch_remote_name(
		NULL, 0, g_repo, unknown), GIT_ENOTFOUND);
}

void test_refs_branches_remote__local_remote_returns_error(void)
{
	const char *local = "refs/heads/master";

	cl_git_fail_with(git_branch_remote_name(
		NULL, 0, g_repo, local), GIT_ERROR);
}

void test_refs_branches_remote__ambiguous_remote_returns_error(void)
{
	git_remote *remote;

	/* Create the remote */
	cl_git_pass(git_remote_create(&remote, g_repo, "addtest", "http://github.com/libgit2/libgit2"));

	/* Update the remote fetch spec */
	cl_git_pass(git_remote_set_fetchspec(remote, "refs/heads/*:refs/remotes/test/*"));
	cl_git_pass(git_remote_save(remote));

	git_remote_free(remote);

	cl_git_fail_with(git_branch_remote_name(NULL, 0, g_repo,
		remote_tracking_branch_name), GIT_EAMBIGUOUS);
}
