#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"

static git_repository *g_repo;
static git_checkout_opts g_opts;
static git_object *g_object;

void test_checkout_tree__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");

	GIT_INIT_STRUCTURE(&g_opts, GIT_CHECKOUT_OPTS_VERSION);
	g_opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
}

void test_checkout_tree__cleanup(void)
{
	git_object_free(g_object);
	g_object = NULL;

	cl_git_sandbox_cleanup();
}

void test_checkout_tree__cannot_checkout_a_non_treeish(void)
{
	/* blob */
	cl_git_pass(git_revparse_single(&g_object, g_repo, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));

	cl_git_fail(git_checkout_tree(g_repo, g_object, NULL));
}

void test_checkout_tree__can_checkout_a_subdirectory_from_a_commit(void)
{
	char *entries[] = { "ab/de/" };

	g_opts.paths.strings = entries;
	g_opts.paths.count = 1;

	cl_git_pass(git_revparse_single(&g_object, g_repo, "subtrees"));

	cl_assert_equal_i(false, git_path_isdir("./testrepo/ab/"));

	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/2.txt"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/fgh/1.txt"));
}

void test_checkout_tree__can_checkout_and_remove_directory(void)
{
	git_reference *head;
	cl_assert_equal_i(false, git_path_isdir("./testrepo/ab/"));

	// Checkout brach "subtrees" and update HEAD, so that HEAD matches the current working tree
	cl_git_pass(git_revparse_single(&g_object, g_repo, "subtrees"));
	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));
	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_git_pass(git_reference_symbolic_set_target(head, "refs/heads/subtrees"));
	git_reference_free(head);
	
	cl_assert_equal_i(true, git_path_isdir("./testrepo/ab/"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/2.txt"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/fgh/1.txt"));

	// Checkout brach "master" and update HEAD, so that HEAD matches the current working tree
	cl_git_pass(git_revparse_single(&g_object, g_repo, "master"));
	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));
	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_git_pass(git_reference_symbolic_set_target(head, "refs/heads/master"));
	git_reference_free(head);

	// This directory should no longer exist
	cl_assert_equal_i(false, git_path_isdir("./testrepo/ab/"));
}

void test_checkout_tree__can_checkout_a_subdirectory_from_a_subtree(void)
{
	char *entries[] = { "de/" };

	g_opts.paths.strings = entries;
	g_opts.paths.count = 1;

	cl_git_pass(git_revparse_single(&g_object, g_repo, "subtrees:ab"));

	cl_assert_equal_i(false, git_path_isdir("./testrepo/de/"));

	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_assert_equal_i(true, git_path_isfile("./testrepo/de/2.txt"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/de/fgh/1.txt"));
}

static void progress(const char *path, size_t cur, size_t tot, void *payload)
{
	bool *was_called = (bool*)payload;
	GIT_UNUSED(path); GIT_UNUSED(cur); GIT_UNUSED(tot);
	*was_called = true;
}

void test_checkout_tree__calls_progress_callback(void)
{
	bool was_called = 0;

	g_opts.progress_cb = progress;
	g_opts.progress_payload = &was_called;

	cl_git_pass(git_revparse_single(&g_object, g_repo, "master"));

	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_assert_equal_i(was_called, true);
}

void test_checkout_tree__doesnt_write_unrequested_files_to_worktree(void)
{
  git_oid master_oid;
  git_oid chomped_oid;
  git_commit* p_master_commit;
  git_commit* p_chomped_commit;
  git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

  git_oid_fromstr(&master_oid, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
  git_oid_fromstr(&chomped_oid, "e90810b8df3e80c413d903f631643c716887138d");
  cl_git_pass(git_commit_lookup(&p_master_commit, g_repo, &master_oid));
  cl_git_pass(git_commit_lookup(&p_chomped_commit, g_repo, &chomped_oid));

  /* GIT_CHECKOUT_NONE should not add any file to the working tree from the
   * index as it is supposed to be a dry run.
   */
  opts.checkout_strategy = GIT_CHECKOUT_NONE;
  git_checkout_tree(g_repo, (git_object*)p_chomped_commit, &opts);
  cl_assert_equal_i(false, git_path_isfile("testrepo/readme.txt"));
}
