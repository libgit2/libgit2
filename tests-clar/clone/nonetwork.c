#include "clar_libgit2.h"

#include "git2/clone.h"
#include "remote.h"
#include "fileops.h"
#include "repository.h"

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"

static git_clone_options g_options;
static git_repository *g_repo;
static git_reference* g_ref;
static git_remote* g_remote;

void test_clone_nonetwork__initialize(void)
{
	git_checkout_opts dummy_opts = GIT_CHECKOUT_OPTS_INIT;
	git_remote_callbacks dummy_callbacks = GIT_REMOTE_CALLBACKS_INIT;

	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
	g_options.checkout_opts = dummy_opts;
	g_options.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	g_options.remote_callbacks = dummy_callbacks;
}

void test_clone_nonetwork__cleanup(void)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	if (g_ref) {
		git_reference_free(g_ref);
		g_ref = NULL;
	}

	if (g_remote) {
		git_remote_free(g_remote);
		g_remote = NULL;
	}

	cl_fixture_cleanup("./foo");
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

void test_clone_nonetwork__do_not_clean_existing_directory(void)
{
	/* Clone should not remove the directory if it already exists, but
	 * Should clean up entries it creates. */
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(git_path_is_empty_dir("./foo"));

	/* Try again with a bare repository. */
	g_options.bare = true;
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(git_path_is_empty_dir("./foo"));
}

void test_clone_nonetwork__local(void)
{
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__local_absolute_path(void)
{
	const char *local_src;
	local_src = cl_fixture("testrepo.git");
	cl_git_pass(git_clone(&g_repo, local_src, "./foo", &g_options));
}

void test_clone_nonetwork__local_bare(void)
{
	g_options.bare = true;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_when_the_target_is_a_file(void)
{
	cl_git_mkfile("./foo", "Bar!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_with_already_existing_but_non_empty_directory(void)
{
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_mkfile("./foo/bar", "Baz!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__custom_origin_name(void)
{
       g_options.remote_name = "my_origin";
       cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

       cl_git_pass(git_remote_load(&g_remote, g_repo, "my_origin"));
}

void test_clone_nonetwork__defaults(void)
{
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", NULL));
	cl_assert(g_repo);
	cl_git_pass(git_remote_load(&g_remote, g_repo, "origin"));
}

void test_clone_nonetwork__cope_with_already_existing_directory(void)
{
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__can_prevent_the_checkout_of_a_standard_repo(void)
{
	git_buf path = GIT_BUF_INIT;

	g_options.checkout_opts.checkout_strategy = 0;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "master.txt"));
	cl_assert_equal_i(false, git_path_isfile(git_buf_cstr(&path)));

	git_buf_free(&path);
}

void test_clone_nonetwork__can_checkout_given_branch(void)
{
	g_options.checkout_branch = "test";
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_assert_equal_i(0, git_repository_head_unborn(g_repo));

	cl_git_pass(git_repository_head(&g_ref, g_repo));
	cl_assert_equal_s(git_reference_name(g_ref), "refs/heads/test");
}

void test_clone_nonetwork__can_detached_head(void)
{
	git_object *obj;
	git_repository *cloned;
	git_reference *cloned_head;

	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_revparse_single(&obj, g_repo, "master~1"));
	cl_git_pass(git_repository_set_head_detached(g_repo, git_object_id(obj)));

	cl_git_pass(git_clone(&cloned, "./foo", "./foo1", &g_options));

	cl_assert(git_repository_head_detached(cloned));

	cl_git_pass(git_repository_head(&cloned_head, cloned));
	cl_assert(!git_oid_cmp(git_object_id(obj), git_reference_target(cloned_head)));

	git_object_free(obj);
	git_reference_free(cloned_head);
	git_repository_free(cloned);

	cl_fixture_cleanup("./foo1");
}
