#include "clar_libgit2.h"
#include "branch.h"
#include "remote.h"

static git_repository *g_repo;

static const char *current_master_tip = "099fabac3a9ea935598528c27f866e34089c2eff";

void test_refs_branches_remote__initialize(void)
{
	git_oid id;

	g_repo = cl_git_sandbox_init("testrepo");
	git_oid_fromstr(&id, current_master_tip);

	/* Create test/master */
	git_reference_create(NULL, g_repo, "refs/remotes/test/master", &id, 1);
}

void test_refs_branches_remote__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_refs_branches_remote__can_get_remote_for_branch(void)
{
	git_reference *ref;
	const char *name;
	char *expectedRemoteName = "test";
	int expectedRemoteNameLength = strlen(expectedRemoteName) + 1;
	char remotename[1024] = {0};

	cl_git_pass(git_branch_lookup(&ref, g_repo, "test/master", GIT_BRANCH_REMOTE));
	cl_git_pass(git_branch_name(&name, ref));
	cl_assert_equal_s("test/master", name);

	cl_assert_equal_i(expectedRemoteNameLength,
		git_branch_remote_name(NULL, 0, g_repo, ref));
	cl_assert_equal_i(expectedRemoteNameLength,
		git_branch_remote_name(remotename, expectedRemoteNameLength, g_repo, ref));
	cl_assert_equal_s("test", remotename);

	git_reference_free(ref);
}

void test_refs_branches_remote__insufficient_buffer_returns_error(void)
{
	git_reference *ref;
	const char *name;
	char *expectedRemoteName = "test";
	int expectedRemoteNameLength = strlen(expectedRemoteName) + 1;
	char remotename[1024] = {0};

	cl_git_pass(git_branch_lookup(&ref, g_repo, "test/master", GIT_BRANCH_REMOTE));
	cl_git_pass(git_branch_name(&name, ref));
	cl_assert_equal_s("test/master", name);

	cl_assert_equal_i(expectedRemoteNameLength,
		git_branch_remote_name(NULL, 0, g_repo, ref));
	cl_git_fail_with(GIT_ERROR,
		git_branch_remote_name(remotename, expectedRemoteNameLength - 1, g_repo, ref));

	git_reference_free(ref);
}

void test_refs_branches_remote__no_matching_remote_returns_error(void)
{
	git_reference *ref;
	const char *name;
	git_oid id;

	git_oid_fromstr(&id, current_master_tip);

	/* Create nonexistent/master */
	git_reference_create(NULL, g_repo, "refs/remotes/nonexistent/master", &id, 1);

	cl_git_pass(git_branch_lookup(&ref, g_repo,"nonexistent/master", GIT_BRANCH_REMOTE));
	cl_git_pass(git_branch_name(&name, ref));
	cl_assert_equal_s("nonexistent/master", name);

	cl_git_fail_with(git_branch_remote_name(NULL, 0, g_repo, ref), GIT_ENOTFOUND);
	git_reference_free(ref);
}

void test_refs_branches_remote__local_remote_returns_error(void)
{
	git_reference *ref;
	const char *name;

	cl_git_pass(git_branch_lookup(&ref,g_repo, "master", GIT_BRANCH_LOCAL));
	cl_git_pass(git_branch_name(&name, ref));
	cl_assert_equal_s("master",name);

	cl_git_fail_with(git_branch_remote_name(NULL, 0, g_repo, ref), GIT_ERROR);
	git_reference_free(ref);
}

void test_refs_branches_remote__ambiguous_remote_returns_error(void)
{
	git_reference *ref;
	const char *name;
	git_remote *remote;

	/* Create the remote */
	cl_git_pass(git_remote_create(&remote, g_repo, "addtest", "http://github.com/libgit2/libgit2"));

	/* Update the remote fetch spec */
	cl_git_pass(git_remote_set_fetchspec(remote, "refs/heads/*:refs/remotes/test/*"));
	cl_git_pass(git_remote_save(remote));

	git_remote_free(remote);

	cl_git_pass(git_branch_lookup(&ref,g_repo, "test/master", GIT_BRANCH_REMOTE));
	cl_git_pass(git_branch_name(&name, ref));
	cl_assert_equal_s("test/master", name);

	cl_git_fail_with(git_branch_remote_name(NULL, 0, g_repo, ref), GIT_EAMBIGUOUS);
	git_reference_free(ref);
}
