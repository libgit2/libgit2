#include "clar_libgit2.h"

#include "git2/clone.h"
#include "futils.h"
#include "fs_path.h"
#include "repo/repo_helpers.h"

static git_repository *g_repo;

void test_clone_bundle__initialize(void)
{
	g_repo = NULL;
}

void test_clone_bundle__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;

	cl_fixture_cleanup("tmp_global_path");
	cl_fixture_cleanup("./bundle-clone");
}

static void clone_bundle(const char *fixture)
{
	cl_git_pass(git_clone(&g_repo, cl_fixture(fixture),
		"./bundle-clone", NULL));
}

void test_clone_bundle__self_contained_sha1(void)
{
	git_reference *head;
	git_oid expected;
	git_commit *commit;
	git_buf upstream = GIT_BUF_INIT;

	clone_bundle("bundle/testrepo.bundle");

	/* the recorded HEAD resolves to master through the usual logic */
	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));
	git_reference_free(head);

	cl_git_pass(git_branch_upstream_name(&upstream, g_repo, "refs/heads/master"));
	cl_assert_equal_s("refs/remotes/origin/master", upstream.ptr);
	git_buf_dispose(&upstream);

	/* remote-tracking references were created from the advertisement */
	cl_git_pass(git_reference_lookup(&head, g_repo, "refs/remotes/origin/br2"));
	git_reference_free(head);
	cl_git_pass(git_reference_lookup(&head, g_repo, "refs/remotes/origin/HEAD"));
	git_reference_free(head);

	/* the objects really arrived */
	cl_git_pass(git_oid_from_string(&expected,
		"a65fedf39aefe402d3bb6e24df4d4f5fe4547750", GIT_OID_SHA1));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &expected));
	git_commit_free(commit);

	/* and the working directory was populated */
	cl_assert(git_fs_path_exists("./bundle-clone/README"));
}

/*
 * The bundle records no HEAD.  Its only branch is the configured
 * initial branch, so clone selects and checks it out, the way Git does.
 */
void test_clone_bundle__no_head_selects_initial_branch(void)
{
	git_reference *ref;
	git_buf upstream = GIT_BUF_INIT;

	clone_bundle("bundle/nohead.bundle");

	cl_git_pass(git_reference_lookup(&ref, g_repo, "HEAD"));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(ref));
	git_reference_free(ref);

	cl_assert_equal_i(0, git_repository_head_unborn(g_repo));

	cl_git_pass(git_branch_upstream_name(&upstream, g_repo, "refs/heads/master"));
	cl_assert_equal_s("refs/remotes/origin/master", upstream.ptr);
	git_buf_dispose(&upstream);

	/* the bundle advertised no HEAD, so none is invented */
	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, g_repo, "refs/remotes/origin/HEAD"));

	cl_assert(git_fs_path_exists("./bundle-clone/README"));
}

/*
 * The configured initial branch is not in the bundle.  Git leaves it
 * unborn rather than checking out the only branch there is.
 */
void test_clone_bundle__no_head_leaves_other_branch_unborn(void)
{
	git_reference *ref;

	create_tmp_global_config("tmp_global_path", "init.defaultbranch", "trunk");

	clone_bundle("bundle/nohead.bundle");

	cl_assert_equal_i(1, git_repository_head_unborn(g_repo));

	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, g_repo, "refs/heads/master"));
	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, g_repo, "refs/heads/trunk"));

	cl_assert(!git_fs_path_exists("./bundle-clone/README"));
}

/* a recorded HEAD that matches no advertised branch clones detached */
void test_clone_bundle__detached_head(void)
{
	git_oid expected, head;

	clone_bundle("bundle/detached.bundle");

	cl_assert_equal_i(1, git_repository_head_detached(g_repo));

	cl_git_pass(git_reference_name_to_id(&head, g_repo, "HEAD"));
	cl_git_pass(git_oid_from_string(&expected,
		"a65fedf39aefe402d3bb6e24df4d4f5fe4547750", GIT_OID_SHA1));
	cl_assert(git_oid_equal(&expected, &head));
}

void test_clone_bundle__self_contained_sha256(void)
{
	git_reference *head;

	clone_bundle("bundle/testrepo_256.bundle");

	cl_assert_equal_i(GIT_OID_SHA256, git_repository_oid_type(g_repo));

	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));
	git_reference_free(head);

	cl_assert(git_fs_path_exists("./bundle-clone/README"));
}

/* an incremental bundle has prerequisites an empty repository lacks */
void test_clone_bundle__incremental_into_empty_fails(void)
{
	cl_git_fail(git_clone(&g_repo, cl_fixture("bundle/incremental.bundle"),
		"./bundle-clone", NULL));

	cl_assert(strstr(git_error_last()->message, "missing") != NULL);

	/* clone cleaned up after itself */
	cl_assert(!git_fs_path_exists("./bundle-clone"));
}
