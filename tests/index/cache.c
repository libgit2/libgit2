#include "clar_libgit2.h"
#include "git2.h"
#include "index.h"
#include "tree-cache.h"

static git_repository *g_repo;

void test_index_cache__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_index_cache__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

void test_index_cache__write_extension_at_root(void)
{
	git_index *index;
	git_tree *tree;
	git_oid id;
	const char *tree_id_str = "45dd856fdd4d89b884c340ba0e047752d9b085d6";
	const char *index_file = "index-tree";

	cl_git_pass(git_index_open(&index, index_file));
	cl_assert(index->tree == NULL);
	cl_git_pass(git_oid_fromstr(&id, tree_id_str));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_index_read_tree(index, tree));
	git_tree_free(tree);

	cl_assert(index->tree);
	cl_git_pass(git_index_write(index));
	git_index_free(index);

	cl_git_pass(git_index_open(&index, index_file));
	cl_assert(index->tree);

	cl_assert_equal_i(0, index->tree->entry_count);
	cl_assert_equal_i(0, index->tree->children_count);

	cl_assert(git_oid_equal(&id, &index->tree->oid));

	cl_git_pass(p_unlink(index_file));
	git_index_free(index);
}

void test_index_cache__write_extension_invalidated_root(void)
{
	git_index *index;
	git_tree *tree;
	git_oid id;
	const char *tree_id_str = "45dd856fdd4d89b884c340ba0e047752d9b085d6";
	const char *index_file = "index-tree-invalidated";
	git_index_entry entry;

	cl_git_pass(git_index_open(&index, index_file));
	cl_assert(index->tree == NULL);
	cl_git_pass(git_oid_fromstr(&id, tree_id_str));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_index_read_tree(index, tree));
	git_tree_free(tree);

	cl_assert(index->tree);

	memset(&entry, 0x0, sizeof(git_index_entry));
	git_oid_cpy(&entry.id, &git_index_get_byindex(index, 0)->id);
	entry.mode = GIT_FILEMODE_BLOB;
	entry.path = "some-new-file.txt";

	cl_git_pass(git_index_add(index, &entry));

	cl_assert_equal_i(-1, index->tree->entry_count);

	cl_git_pass(git_index_write(index));
	git_index_free(index);

	cl_git_pass(git_index_open(&index, index_file));
	cl_assert(index->tree);

	cl_assert_equal_i(-1, index->tree->entry_count);
	cl_assert_equal_i(0, index->tree->children_count);

	cl_assert(git_oid_cmp(&id, &index->tree->oid));

	cl_git_pass(p_unlink(index_file));
	git_index_free(index);
}

void test_index_cache__read_tree_no_children(void)
{
	git_index *index;
	git_index_entry entry;
	git_tree *tree;
	git_oid id;

	cl_git_pass(git_index_new(&index));
	cl_assert(index->tree == NULL);
	cl_git_pass(git_oid_fromstr(&id, "45dd856fdd4d89b884c340ba0e047752d9b085d6"));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_index_read_tree(index, tree));
	git_tree_free(tree);

	cl_assert(index->tree);
	cl_assert(git_oid_equal(&id, &index->tree->oid));
	cl_assert_equal_i(0, index->tree->children_count);
	cl_assert_equal_i(0, index->tree->entry_count); /* 0 is a placeholder here */

	memset(&entry, 0x0, sizeof(git_index_entry));
	entry.path = "new.txt";
	entry.mode = GIT_FILEMODE_BLOB;
	git_oid_fromstr(&entry.id, "45b983be36b73c0788dc9cbcb76cbb80fc7bb057");

	cl_git_pass(git_index_add(index, &entry));
	cl_assert_equal_i(-1, index->tree->entry_count);

	git_index_free(index);
}

void test_index_cache__read_tree_children(void)
{
	git_index *index;
	git_index_entry entry;
	git_tree *tree;
	const git_tree_cache *cache;
	git_oid tree_id;

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_clear(index));
	cl_assert(index->tree == NULL);


	/* add a bunch of entries at different levels */
	memset(&entry, 0x0, sizeof(git_index_entry));
	entry.path = "top-level";
	entry.mode = GIT_FILEMODE_BLOB;
	git_oid_fromstr(&entry.id, "45b983be36b73c0788dc9cbcb76cbb80fc7bb057");
	cl_git_pass(git_index_add(index, &entry));


	entry.path = "subdir/some-file";
	cl_git_pass(git_index_add(index, &entry));

	entry.path = "subdir/even-deeper/some-file";
	cl_git_pass(git_index_add(index, &entry));

	entry.path = "subdir2/some-file";
	cl_git_pass(git_index_add(index, &entry));

	cl_git_pass(git_index_write_tree(&tree_id, index));
	cl_git_pass(git_index_clear(index));
	cl_assert(index->tree == NULL);

	cl_git_pass(git_tree_lookup(&tree, g_repo, &tree_id));
	cl_git_pass(git_index_read_tree(index, tree));
	git_tree_free(tree);

	cl_assert(index->tree);
	cl_assert_equal_i(2, index->tree->children_count);

	/* override with a slightly different id, also dummy */
	entry.path = "subdir/some-file";
	git_oid_fromstr(&entry.id, "45b983be36b73c0788dc9cbcb76cbb80fc7bb058");
	cl_git_pass(git_index_add(index, &entry));

	cl_assert_equal_i(-1, index->tree->entry_count);

	cache = git_tree_cache_get(index->tree, "subdir");
	cl_assert(cache);
	cl_assert_equal_i(-1, cache->entry_count);

	cache = git_tree_cache_get(index->tree, "subdir/even-deeper");
	cl_assert(cache);
	cl_assert_equal_i(0, cache->entry_count);

	cache = git_tree_cache_get(index->tree, "subdir2");
	cl_assert(cache);
	cl_assert_equal_i(0, cache->entry_count);

	git_index_free(index);
}
