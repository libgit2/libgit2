#include "clar_libgit2.h"
#include "posix.h"

/* Test that reading and writing a tree is a no-op */
void test_index_read_tree__read_write_involution(void)
{
	git_repository *repo;
	git_index *index;
	git_oid tree_oid;
	git_tree *tree;
	git_oid expected;

	p_mkdir("read_tree", 0700);

	cl_git_pass(git_repository_init(&repo, "./read_tree", 0));
	cl_git_pass(git_repository_index(&index, repo));

	cl_assert(git_index_entrycount(index) == 0);

	p_mkdir("./read_tree/abc", 0700);

	/* Sort order: '-' < '/' < '_' */
	cl_git_mkfile("./read_tree/abc-d", NULL);
	cl_git_mkfile("./read_tree/abc/d", NULL);
	cl_git_mkfile("./read_tree/abc_d", NULL);

	cl_git_pass(git_index_add(index, "abc-d", 0));
	cl_git_pass(git_index_add(index, "abc_d", 0));
	cl_git_pass(git_index_add(index, "abc/d", 0));

	/* write-tree */
	cl_git_pass(git_tree_create_fromindex(&expected, index));

	/* read-tree */
	git_tree_lookup(&tree, repo, &expected);
	cl_git_pass(git_index_read_tree(index, tree));
	git_tree_free(tree);

	cl_git_pass(git_tree_create_fromindex(&tree_oid, index));
	cl_assert(git_oid_cmp(&expected, &tree_oid) == 0);

	git_index_free(index);
	git_repository_free(repo);

	cl_fixture_cleanup("read_tree");
}
