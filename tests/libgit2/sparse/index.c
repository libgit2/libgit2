#include "clar_libgit2.h"
#include "futils.h"
#include "sparse.h"
#include "index.h"

static git_repository *g_repo = NULL;

void test_sparse_index__initialize(void)
{
}

void test_sparse_index__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_sparse_index__add_bypath(void)
{
	git_index* index;
	const git_index_entry* entry;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_git_pass(git_repository_index(&index, g_repo));
	
	cl_git_mkfile("sparse/newfile", "/hello world\n");
	cl_git_pass(git_index_add_bypath(index, "newfile"));
	cl_assert(entry = git_index_get_bypath(index, "newfile", 0));
	cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);
	
	git_index_free(index);
}

void test_sparse_index__add_bypath_sparse(void)
{
	git_index* index;
	const git_index_entry* entry;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_git_pass(git_repository_index(&index, g_repo));
	
	cl_must_pass(git_futils_mkdir("sparse/a", 0777, 0));
	cl_git_mkfile("sparse/a/newfile", "/hello world\n");
	cl_git_pass(git_index_add_bypath(index, "a/newfile"));
	cl_assert(entry = git_index_get_bypath(index, "a/newfile", 0));
	cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, GIT_INDEX_ENTRY_SKIP_WORKTREE);
	
	git_index_free(index);
}

void test_sparse_index__add_bypath_disabled_sparse(void)
{
	git_index* index;
	const git_index_entry* entry;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_sparse_checkout_disable(g_repo));

	cl_git_pass(git_repository_index(&index, g_repo));
	
	cl_must_pass(git_futils_mkdir("sparse/a", 0777, 0));
	cl_git_mkfile("sparse/a/newfile", "/hello world\n");
	cl_git_pass(git_index_add_bypath(index, "a/newfile"));
	cl_assert(entry = git_index_get_bypath(index, "a/newfile", 0));
	cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);
	
	git_index_free(index);
}

void test_sparse_index__add_all(void)
{
	git_index* index;
	const git_index_entry* entry;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_git_pass(git_repository_index(&index, g_repo));
	
	cl_git_mkfile("sparse/newfile", "/hello world\n");
	cl_git_pass(git_index_add_all(index, NULL, GIT_INDEX_ADD_DEFAULT, NULL, NULL));
	cl_assert(entry = git_index_get_bypath(index, "newfile", 0));
	cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);
	
	git_index_free(index);
}

void test_sparse_index__add_all_sparse(void)
{
	git_index* index;
	const git_index_entry* entry;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	cl_git_pass(git_repository_index(&index, g_repo));
	
	cl_must_pass(git_futils_mkdir("sparse/a", 0777, 0));
	cl_git_mkfile("sparse/a/newfile", "/hello world\n");
	cl_git_pass(git_index_add_all(index, NULL, GIT_INDEX_ADD_DEFAULT, NULL, NULL));
	cl_assert(entry = git_index_get_bypath(index, "a/newfile", 0));
	cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, GIT_INDEX_ENTRY_SKIP_WORKTREE);
	
	git_index_free(index);
}

void test_sparse_index__add_all_disabled_sparse(void)
{
	git_index* index;
	const git_index_entry* entry;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_sparse_checkout_disable(g_repo));

	cl_git_pass(git_repository_index(&index, g_repo));
	
	cl_must_pass(git_futils_mkdir("sparse/a", 0777, 0));
	cl_git_mkfile("sparse/a/newfile", "/hello world\n");
	cl_git_pass(git_index_add_all(index, NULL, GIT_INDEX_ADD_DEFAULT, NULL, NULL));
	cl_assert(entry = git_index_get_bypath(index, "a/newfile", 0));
	cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);
	
	git_index_free(index);
}

void test_sparse_index__read_tree_sets_skip_worktree(void)
{
	git_index* index;
	git_tree* tree;
	git_oid tree_id;
	const git_index_entry* entry;
	const char** test_file;
	const char *test_files[] = {
		"a/file3",
		"a/file4",
		NULL
	};
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "/a/" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}

	git_oid_fromstr(&tree_id, "466cd582210eceaec48d949c7adaa0ceb2042db6");
	
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &tree_id));
	
	cl_git_pass(git_index_read_tree(index, tree));
	
	for (test_file = test_files; *test_file != NULL; ++test_file) {
		cl_assert(entry = git_index_get_bypath(index, *test_file, 0));
		cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);
	}
	
	git_tree_free(tree);
	git_index_free(index);
}

void test_sparse_index__read_tree_sets_skip_worktree_disabled(void)
{
	git_index* index;
	git_tree* tree;
	git_oid tree_id;
	git_index_iterator* iterator;
	const git_index_entry *entry;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_sparse_checkout_disable(g_repo));

	git_oid_fromstr(&tree_id, "466cd582210eceaec48d949c7adaa0ceb2042db6");
	
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &tree_id));
	
	cl_git_pass(git_index_read_tree(index, tree));
	
	cl_git_pass(git_index_iterator_new(&iterator, index));
	while (git_index_iterator_next(&entry, iterator) != GIT_ITEROVER)
		cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);
	
	git_tree_free(tree);
	git_index_iterator_free(iterator);
	git_index_free(index);
}

void test_sparse_index__read_tree_sets_skip_worktree_all_sparse(void)
{
	git_index* index;
	git_tree* tree;
	git_oid tree_id;
	git_index_iterator* iterator;
	const git_index_entry *entry;
	
	g_repo = cl_git_sandbox_init("sparse");
	git_oid_fromstr(&tree_id, "466cd582210eceaec48d949c7adaa0ceb2042db6");

	{
		char *pattern_strings[] = { "!/*" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_set(g_repo, &patterns));
	}
	
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &tree_id));
	
	cl_git_pass(git_index_read_tree(index, tree));
	
	cl_git_pass(git_index_iterator_new(&iterator, index));
	while (git_index_iterator_next(&entry, iterator) != GIT_ITEROVER)
		cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, GIT_INDEX_ENTRY_SKIP_WORKTREE);
	
	git_tree_free(tree);
	git_index_iterator_free(iterator);
	git_index_free(index);
}
