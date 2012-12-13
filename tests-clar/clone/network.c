#include "clar_libgit2.h"

#include "git2/clone.h"
#include "repository.h"

CL_IN_CATEGORY("network")

#define LIVE_REPO_URL "http://github.com/libgit2/TestGitRepository"
#define LIVE_EMPTYREPO_URL "http://github.com/libgit2/TestEmptyRepository"

static git_repository *g_repo;
static git_remote *g_origin;

void test_clone_network__initialize(void)
{
	g_repo = NULL;
	cl_git_pass(git_remote_new(&g_origin, NULL, "origin", LIVE_REPO_URL, ""));
}

static void cleanup_repository(void *path)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}
	cl_fixture_cleanup((const char *)path);
}


void test_clone_network__network_full(void)
{
	git_remote *origin;

	cl_set_cleanup(&cleanup_repository, "./test2");

	cl_git_pass(git_clone(&g_repo, g_origin, "./test2", NULL, NULL, NULL));
	cl_assert(!git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));

	git_remote_free(origin);
}


void test_clone_network__network_bare(void)
{
	git_remote *origin;

	cl_set_cleanup(&cleanup_repository, "./test");

	cl_git_pass(git_clone_bare(&g_repo, g_origin, "./test", NULL, NULL));
	cl_assert(git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));

	git_remote_free(origin);
}

void test_clone_network__cope_with_already_existing_directory(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_pass(git_clone(&g_repo, g_origin, "./foo", NULL, NULL, NULL));
	git_repository_free(g_repo); g_repo = NULL;
}

void test_clone_network__empty_repository(void)
{
	git_reference *head;

	cl_set_cleanup(&cleanup_repository, "./empty");

	git_remote_free(g_origin);
	cl_git_pass(git_remote_new(&g_origin, NULL, "origin", LIVE_EMPTYREPO_URL, ""));

	cl_git_pass(git_clone(&g_repo, g_origin, "./empty", NULL, NULL, NULL));

	cl_assert_equal_i(true, git_repository_is_empty(g_repo));
	cl_assert_equal_i(true, git_repository_head_orphan(g_repo));

	cl_git_pass(git_reference_lookup(&head, g_repo, GIT_HEAD_FILE));
	cl_assert_equal_i(GIT_REF_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));

	git_reference_free(head);
}

void test_clone_network__can_prevent_the_checkout_of_a_standard_repo(void)
{
	git_buf path = GIT_BUF_INIT;

	cl_set_cleanup(&cleanup_repository, "./no-checkout");

	cl_git_pass(git_clone(&g_repo, g_origin, "./no-checkout", NULL, NULL, NULL));

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "master.txt"));
	cl_assert_equal_i(false, git_path_isfile(git_buf_cstr(&path)));

	git_buf_free(&path);
}

static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
	bool *was_called = (bool*)payload;
	GIT_UNUSED(path); GIT_UNUSED(cur); GIT_UNUSED(tot);
	(*was_called) = true;
}

static void fetch_progress(const git_transfer_progress *stats, void *payload)
{
	bool *was_called = (bool*)payload;
	GIT_UNUSED(stats);
	(*was_called) = true;
}

void test_clone_network__can_checkout_a_cloned_repo(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	git_buf path = GIT_BUF_INIT;
	git_reference *head;
	bool checkout_progress_cb_was_called = false,
		  fetch_progress_cb_was_called = false;

	opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	opts.progress_cb = &checkout_progress;
	opts.progress_payload = &checkout_progress_cb_was_called;

	cl_set_cleanup(&cleanup_repository, "./default-checkout");

	cl_git_pass(git_clone(&g_repo, g_origin, "./default-checkout", &opts,
				&fetch_progress, &fetch_progress_cb_was_called));

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "master.txt"));
	cl_assert_equal_i(true, git_path_isfile(git_buf_cstr(&path)));

	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_assert_equal_i(GIT_REF_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));

	cl_assert_equal_i(true, checkout_progress_cb_was_called);
	cl_assert_equal_i(true, fetch_progress_cb_was_called);

	git_reference_free(head);
	git_buf_free(&path);
}
