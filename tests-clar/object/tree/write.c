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

	cl_git_fail(git_treebuilder_insert(NULL, builder, "",
		&bid, GIT_FILEMODE_BLOB));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "/",
		&bid, GIT_FILEMODE_BLOB));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "folder/new.txt",
		&bid, GIT_FILEMODE_BLOB));

	cl_git_pass(git_treebuilder_insert(
		NULL, builder, "new.txt", &bid, GIT_FILEMODE_BLOB));

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
	cl_git_pass(git_treebuilder_insert(
		NULL, builder, "new.txt", &bid, GIT_FILEMODE_BLOB)); //-V536
	cl_git_pass(git_treebuilder_write(&subtree_id, g_repo, builder));
	git_treebuilder_free(builder);

	// create parent tree
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_treebuilder_create(&builder, tree));
	cl_git_pass(git_treebuilder_insert(
		NULL, builder, "new", &subtree_id, GIT_FILEMODE_TREE)); //-V536
	cl_git_pass(git_treebuilder_write(&id_hiearar, g_repo, builder));
	git_treebuilder_free(builder);
	git_tree_free(tree);

	cl_assert(git_oid_cmp(&id_hiearar, &id3) == 0);

	// check data is correct
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id_hiearar));
	cl_assert(2 == git_tree_entrycount(tree));
	git_tree_free(tree);
}

/*
 * And the Lord said: Is this tree properly sorted?
 */
void test_object_tree_write__sorted_subtrees(void)
{
	git_treebuilder *builder;
	unsigned int i;
	int position_c = -1, position_cake = -1, position_config = -1;

	struct {
		unsigned int attr;
		const char *filename;
	} entries[] = {
		{ GIT_FILEMODE_BLOB, ".gitattributes" },
	  	{ GIT_FILEMODE_BLOB, ".gitignore" },
	  	{ GIT_FILEMODE_BLOB, ".htaccess" },
	  	{ GIT_FILEMODE_BLOB, "Capfile" },
	  	{ GIT_FILEMODE_BLOB, "Makefile"},
	  	{ GIT_FILEMODE_BLOB, "README"},
	  	{ GIT_FILEMODE_TREE, "app"},
	  	{ GIT_FILEMODE_TREE, "cake"},
	  	{ GIT_FILEMODE_TREE, "config"},
	  	{ GIT_FILEMODE_BLOB, "c"},
	  	{ GIT_FILEMODE_BLOB, "git_test.txt"},
	  	{ GIT_FILEMODE_BLOB, "htaccess.htaccess"},
	  	{ GIT_FILEMODE_BLOB, "index.php"},
	  	{ GIT_FILEMODE_TREE, "plugins"},
	  	{ GIT_FILEMODE_TREE, "schemas"},
	  	{ GIT_FILEMODE_TREE, "ssl-certs"},
	  	{ GIT_FILEMODE_TREE, "vendors"}
	};

	git_oid blank_oid, tree_oid;

	memset(&blank_oid, 0x0, sizeof(blank_oid));

	cl_git_pass(git_treebuilder_create(&builder, NULL));

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
		cl_git_pass(git_treebuilder_insert(NULL,
			builder, entries[i].filename, &blank_oid, entries[i].attr));
	}

	cl_git_pass(git_treebuilder_write(&tree_oid, g_repo, builder));

	for (i = 0; i < builder->entries.length; ++i) {
		git_tree_entry *entry = git_vector_get(&builder->entries, i);

		if (strcmp(entry->filename, "c") == 0)
			position_c = i;

		if (strcmp(entry->filename, "cake") == 0)
			position_cake = i;

		if (strcmp(entry->filename, "config") == 0)
			position_config = i;
	}

	cl_assert(position_c != -1);
	cl_assert(position_cake != -1);
	cl_assert(position_config != -1);

	cl_assert(position_c < position_cake);
	cl_assert(position_cake < position_config);

	git_treebuilder_free(builder);
}
