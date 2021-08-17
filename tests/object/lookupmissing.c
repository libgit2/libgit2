#include "clar_libgit2.h"

#include "repository.h"

static git_repository *g_repo;
static git_tree *g_root_tree;
static git_object *g_result_object;
static git_tree_entry *g_result_entry;

/*
 * Looking up an object by path involves repeating the following two operations:
 * 1. Find tree-entry of the next path-name in current tree object.
 * 2. Find associated tree/blob object by OID in ODB.
 *
 * Normally, looking up an object by path fails with ENOTFOUND when it becomes
 * clear that a tree-entry of the required name doesn't exist (step 1).
 *
 * However, in certain circumstances, step 2 can fail. Ordinarily it should not
 * fail, but it can if a) the ODB is corrupted or b) the ODB only contains a
 * partial clone. This file is for testing this type of failure.
 */


void test_object_lookupmissing__initialize(void)
{
	git_reference *head;

	cl_git_pass(git_repository_open(&g_repo, cl_fixture("partial-clone.git")));

	cl_git_pass(git_repository_head(&head, g_repo));
	cl_git_pass(git_reference_peel((git_object**)&g_root_tree, head, GIT_OBJECT_TREE));

	git_reference_free(head);

}
void test_object_lookupmissing__cleanup(void)
{
	git_object_free(g_result_object);
	g_result_object = NULL;
	git_tree_free(g_root_tree);
	g_root_tree = NULL;
	git_repository_free(g_repo);
	g_repo = NULL;
}

void test_object_lookupmissing__missing(void)
{
	/* files/first/large_file is missing, and it's not clear why it is missing
	 * from a packfile that is not marked as being a promisor-packfile. */

	/* Path -> object. */
	cl_assert_equal_i(GIT_EMISSING,
		git_object_lookup_bypath(&g_result_object, (git_object*)g_root_tree,
			"files/first/large_file", GIT_OBJECT_ANY));

	/* Path -> tree-entry -> object. */
	cl_git_pass(git_tree_entry_bypath(&g_result_entry, g_root_tree,
		"files/first/large_file"));
	cl_assert_equal_i(GIT_EMISSING,
		git_tree_entry_to_object(&g_result_object, g_repo, g_result_entry));
}

void test_object_lookupmissing__missing_with_promisor(void)
{
	/* files/second/large_file is missing from a promisor packfile -
	 * so probably is available at the remote (ie, a partial clone) */

	/* Path -> object. */
	/* TODO: add a new error code for this - EPROMISED. */
	cl_assert_equal_i(GIT_EMISSING,
		git_object_lookup_bypath(&g_result_object, (git_object*)g_root_tree,
			"files/second/large_file", GIT_OBJECT_ANY));

	/* Path -> tree-entry -> object. */
	cl_git_pass(git_tree_entry_bypath(&g_result_entry, g_root_tree,
		"files/second/large_file"));
	/* TODO: add a new error code for this - EPROMISED. */
	cl_assert_equal_i(GIT_EMISSING,
		git_tree_entry_to_object(&g_result_object, g_repo, g_result_entry));
}

void test_object_lookupmissing__missing_commit_tree(void)
{
	git_reference* branch;
	git_commit* commit;
	git_tree* tree;

	cl_git_pass(git_branch_lookup(
		&branch, g_repo, "unpeelable-commit", GIT_BRANCH_LOCAL));
	cl_git_pass(git_reference_peel(
		(git_object**)&commit, branch, GIT_OBJECT_COMMIT));

	/* commit -> tree. */
	cl_assert_equal_i(GIT_EMISSING, git_commit_tree(&tree, commit));

	/* peel(commit) -> tree */
	cl_assert_equal_i(GIT_EMISSING,
		git_object_peel((git_object**)&tree, (git_object*)commit, GIT_OBJECT_TREE));

	/* peel(branch) -> tree */
	cl_assert_equal_i(GIT_EMISSING,
		git_reference_peel((git_object**)&tree, branch, GIT_OBJECT_TREE));

	git_commit_free(commit);
	git_reference_free(branch);

}

void test_object_lookupmissing__normal(void)
{
	/* Make sure that lookups are otherwise still working as normal in this
	 * incomplete packfile / incomplete promisor-packfile. */

	cl_git_pass(
		git_object_lookup_bypath(&g_result_object, (git_object*)g_root_tree,
			"files/first/README", GIT_OBJECT_BLOB));
	git_object_free(g_result_object);

	cl_git_pass(
		git_object_lookup_bypath(&g_result_object, (git_object*)g_root_tree,
			"files/second/README", GIT_OBJECT_BLOB));
	git_object_free(g_result_object);

	cl_assert_equal_i(GIT_ENOTFOUND,
		git_object_lookup_bypath(&g_result_object, (git_object*)g_root_tree,
			"files/first/nonexistent", GIT_OBJECT_ANY));

	cl_assert_equal_i(GIT_ENOTFOUND,
		git_object_lookup_bypath(&g_result_object, (git_object*)g_root_tree,
			"files/second/nonexistent", GIT_OBJECT_ANY));
}

