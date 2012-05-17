#include "clar_libgit2.h"

static git_repository *repo;
const char *tree_with_subtrees_oid = "ae90f12eea699729ed24555e40b9fd669da12a12";
static	git_tree *tree;

void test_object_tree_frompath__initialize(void)
{
	git_oid id;

	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&repo, "testrepo.git"));
	cl_assert(repo != NULL);

	cl_git_pass(git_oid_fromstr(&id, tree_with_subtrees_oid));
	cl_git_pass(git_tree_lookup(&tree, repo, &id));
	cl_assert(tree != NULL);
}

void test_object_tree_frompath__cleanup(void)
{
	git_tree_free(tree);
	git_repository_free(repo);
	cl_fixture_cleanup("testrepo.git");
}

static void assert_tree_from_path(git_tree *root, const char *path, int expected_result, const char *expected_raw_oid)
{
	git_tree *containing_tree = NULL;

	cl_assert(git_tree_get_subtree(&containing_tree, root, path) == expected_result);
	
	if (containing_tree == NULL && expected_result != 0)
		return;
	
	cl_assert(containing_tree != NULL && expected_result == 0);

	cl_git_pass(git_oid_streq(git_object_id((const git_object *)containing_tree), expected_raw_oid));

	git_tree_free(containing_tree);
}

static void assert_tree_from_path_klass(git_tree *root, const char *path, int expected_result, const char *expected_raw_oid)
{
	assert_tree_from_path(root, path, GIT_ERROR, expected_raw_oid);
	cl_assert(giterr_last()->klass == expected_result);
}

void test_object_tree_frompath__retrieve_tree_from_path_to_treeentry(void)
{
	/* Will return self if given a one path segment... */
	assert_tree_from_path(tree, "README", 0, tree_with_subtrees_oid);
	
	/* ...even one that lead to a non existent tree entry. */
	assert_tree_from_path(tree, "i-do-not-exist.txt", 0, tree_with_subtrees_oid);
	
	/* Will return fgh tree oid given this following path... */
	assert_tree_from_path(tree, "ab/de/fgh/1.txt", 0, "3259a6bd5b57fb9c1281bb7ed3167b50f224cb54");
	
	/* ... and ab tree oid given this one. */
	assert_tree_from_path(tree, "ab/de", 0, "f1425cef211cc08caa31e7b545ffb232acb098c3");

	/* Will succeed if given a valid path which leads to a tree entry which doesn't exist */
	assert_tree_from_path(tree, "ab/de/fgh/i-do-not-exist.txt", 0, "3259a6bd5b57fb9c1281bb7ed3167b50f224cb54");
}

void test_object_tree_frompath__fail_when_processing_an_unknown_tree_segment(void)
{
	assert_tree_from_path(tree, "nope/de/fgh/1.txt", GIT_ENOTFOUND, NULL);
	assert_tree_from_path(tree, "ab/me-neither/fgh/2.txt", GIT_ENOTFOUND, NULL);
}

void test_object_tree_frompath__fail_when_processing_an_invalid_path(void)
{
	assert_tree_from_path_klass(tree, "/", GITERR_INVALID, NULL);
	assert_tree_from_path_klass(tree, "/ab", GITERR_INVALID, NULL);
	assert_tree_from_path_klass(tree, "/ab/de", GITERR_INVALID, NULL);
	assert_tree_from_path_klass(tree, "ab/", GITERR_INVALID, NULL);
	assert_tree_from_path_klass(tree, "ab//de", GITERR_INVALID, NULL);
	assert_tree_from_path_klass(tree, "ab/de/", GITERR_INVALID, NULL);
}
