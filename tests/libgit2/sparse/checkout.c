#include "clar_libgit2.h"
#include "futils.h"
#include "sparse.h"
#include "git2/checkout.h"
#include "commit.h"

static git_repository *g_repo = NULL;

void test_sparse_checkout__initialize(void)
{
}

void test_sparse_checkout__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void checkout_first_commit(void)
{
	git_object *obj;
	const char *commit_sha = "35e0dddab1fda55a937272c72c941e1877a47300";
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

	cl_git_pass(git_revparse_single(&obj, g_repo, commit_sha));

	opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	cl_git_pass(git_checkout_tree(g_repo, obj, &opts));
	cl_git_pass(git_repository_set_head_detached(g_repo, git_object_id(obj)));

	git_object_free(obj);
}

void checkout_head(void)
{
	git_object *obj;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

	cl_git_pass(git_revparse_single(&obj, g_repo, "main"));
	cl_git_pass(git_checkout_tree(g_repo, obj, &opts));
	cl_git_pass(git_repository_set_head(g_repo, "refs/heads/main"));

	git_object_free(obj);
}

void test_sparse_checkout__skips_sparse_files(void)
{
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	checkout_first_commit();

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	checkout_head();
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/file5"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/c/file7"), false);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/d/file9"), false);
}

void test_sparse_checkout__checksout_files(void)
{
	char* pattern_strings[] = { "/a/" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	checkout_first_commit();

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));

	checkout_head();
	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
}

void test_sparse_checkout__checksout_all_files(void)
{
    char *pattern_strings[] = { "/*" };
    git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	g_repo = cl_git_sandbox_init("sparse");

	checkout_first_commit();

    cl_git_pass(git_sparse_checkout_set(g_repo, &patterns));

	checkout_head();
	cl_assert_equal_b(git_fs_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/a/file3"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/file5"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/c/file7"), true);
	cl_assert_equal_b(git_fs_path_exists("sparse/b/d/file9"), true);
}

void test_sparse_checkout__updates_index(void)
{
	char *pattern_strings[] = { "/*" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_index_iterator* iterator;
	git_index* index;
	const git_index_entry *entry;
	g_repo = cl_git_sandbox_init("sparse");

	checkout_first_commit();

	cl_git_pass(git_sparse_checkout_set(g_repo, &patterns));

	checkout_head();
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_iterator_new(&iterator, index));
	while (git_index_iterator_next(&entry, iterator) != GIT_ITEROVER)
		cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);

	git_index_iterator_free(iterator);
	git_index_free(index);
}