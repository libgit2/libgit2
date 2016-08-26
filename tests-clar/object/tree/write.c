#include "clar_libgit2.h"

#include "tree.h"

static const char *blob_oid = "fa49b077972391ad58037050f2a75f74e3671e92";
static const char *first_tree  = "181037049a54a1eb5fab404658a3a250b44335d7";
static const char *second_tree = "f60079018b664e4e79329a7ef9559c8d9e0378d1";
static const char *third_tree = "eb86d8b81d6adbd5290a935d6c9976882de98488";

static git_repository *g_repo;

// Fixture setup and teardown
void test_object_tree_write__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_object_tree_write__cleanup(void)
{
   cl_git_sandbox_cleanup();
}

void test_object_tree_write__from_memory(void)
{
   // write a tree from a memory
	git_treebuilder *builder;
	git_tree *tree;
	git_oid id, bid, rid, id2;

	git_oid_fromstr(&id, first_tree);
	git_oid_fromstr(&id2, second_tree);
	git_oid_fromstr(&bid, blob_oid);

	//create a second tree from first tree using `git_treebuilder_insert` on REPOSITORY_FOLDER.
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_treebuilder_create(&builder, tree));

	cl_git_fail(git_treebuilder_insert(NULL, builder, "", &bid, 0100644));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "/", &bid, 0100644));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "folder/new.txt", &bid, 0100644));

	cl_git_pass(git_treebuilder_insert(NULL,builder,"new.txt",&bid,0100644));
	cl_git_pass(git_treebuilder_write(&rid, g_repo, builder));

	cl_assert(git_oid_cmp(&rid, &id2) == 0);

	git_treebuilder_free(builder);
	git_tree_free(tree);
}

void test_object_tree_write__subtree(void)
{
   // write a hierarchical tree from a memory
	git_treebuilder *builder;
	git_tree *tree;
	git_oid id, bid, subtree_id, id2, id3;
	git_oid id_hiearar;

	git_oid_fromstr(&id, first_tree);
	git_oid_fromstr(&id2, second_tree);
	git_oid_fromstr(&id3, third_tree);
	git_oid_fromstr(&bid, blob_oid);

	//create subtree
	cl_git_pass(git_treebuilder_create(&builder, NULL));
	cl_git_pass(git_treebuilder_insert(NULL,builder,"new.txt",&bid,0100644));
	cl_git_pass(git_treebuilder_write(&subtree_id, g_repo, builder));
	git_treebuilder_free(builder);

	// create parent tree
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_treebuilder_create(&builder, tree));
	cl_git_pass(git_treebuilder_insert(NULL,builder,"new",&subtree_id,040000));
	cl_git_pass(git_treebuilder_write(&id_hiearar, g_repo, builder));
	git_treebuilder_free(builder);
	git_tree_free(tree);

	cl_assert(git_oid_cmp(&id_hiearar, &id3) == 0);

	// check data is correct
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id_hiearar));
	cl_assert(2 == git_tree_entrycount(tree));
	git_tree_free(tree);
}
