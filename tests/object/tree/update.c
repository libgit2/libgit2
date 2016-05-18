#include "clar_libgit2.h"
#include "tree.h"

static git_repository *g_repo;

void test_object_tree_update__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_object_tree_update__cleanup(void)
{
   cl_git_sandbox_cleanup();
}

void test_object_tree_update__remove_blob(void)
{
	git_oid tree_index_id, tree_updater_id, base_id;
	git_tree *base_tree;
	git_index *idx;
	const char *path = "README";

	git_tree_update updates[] = {
		{ GIT_TREE_UPDATE_REMOVE, {{0}}, GIT_FILEMODE_BLOB /* ignored */, path},
	};

	cl_git_pass(git_oid_fromstr(&base_id, "45dd856fdd4d89b884c340ba0e047752d9b085d6"));
	cl_git_pass(git_tree_lookup(&base_tree, g_repo, &base_id));

	/* Create it with an index */
	cl_git_pass(git_index_new(&idx));
	cl_git_pass(git_index_read_tree(idx, base_tree));
	cl_git_pass(git_index_remove(idx, path, 0));
	cl_git_pass(git_index_write_tree_to(&tree_index_id, idx, g_repo));
	git_index_free(idx);

	/* Perform the same operation via the tree updater */
	cl_git_pass(git_tree_create_updated(&tree_updater_id, g_repo, base_tree, 1, updates));

	cl_assert_equal_oid(&tree_index_id, &tree_updater_id);

	git_tree_free(base_tree);
}

void test_object_tree_update__replace_blob(void)
{
	git_oid tree_index_id, tree_updater_id, base_id;
	git_tree *base_tree;
	git_index *idx;
	const char *path = "README";
	git_index_entry entry = { {0} };

	git_tree_update updates[] = {
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_BLOB, path},
	};

	cl_git_pass(git_oid_fromstr(&base_id, "45dd856fdd4d89b884c340ba0e047752d9b085d6"));
	cl_git_pass(git_tree_lookup(&base_tree, g_repo, &base_id));

	/* Create it with an index */
	cl_git_pass(git_index_new(&idx));
	cl_git_pass(git_index_read_tree(idx, base_tree));

	entry.path = path;
	cl_git_pass(git_oid_fromstr(&entry.id, "3697d64be941a53d4ae8f6a271e4e3fa56b022cc"));
	entry.mode = GIT_FILEMODE_BLOB;
	cl_git_pass(git_index_add(idx, &entry));

	cl_git_pass(git_index_write_tree_to(&tree_index_id, idx, g_repo));
	git_index_free(idx);

	/* Perform the same operation via the tree updater */
	cl_git_pass(git_oid_fromstr(&updates[0].id, "3697d64be941a53d4ae8f6a271e4e3fa56b022cc"));
	cl_git_pass(git_tree_create_updated(&tree_updater_id, g_repo, base_tree, 1, updates));

	cl_assert_equal_oid(&tree_index_id, &tree_updater_id);

	git_tree_free(base_tree);
}

void test_object_tree_update__add_blobs(void)
{
	git_oid tree_index_id, tree_updater_id, base_id;
	git_tree *base_tree;
	git_index *idx;
	git_index_entry entry = { {0} };
	int i;
	const char *paths[] = {
		"some/deep/path",
		"some/other/path",
		"a/path/elsewhere",
	};

	git_tree_update updates[] = {
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_BLOB, paths[0]},
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_BLOB, paths[1]},
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_BLOB, paths[2]},
	};

	cl_git_pass(git_oid_fromstr(&base_id, "45dd856fdd4d89b884c340ba0e047752d9b085d6"));
	cl_git_pass(git_tree_lookup(&base_tree, g_repo, &base_id));

	entry.mode = GIT_FILEMODE_BLOB;
	cl_git_pass(git_oid_fromstr(&entry.id, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));

	for (i = 0; i < 3; i++) {
		cl_git_pass(git_oid_fromstr(&updates[i].id, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));
	}

	for (i = 0; i < 2; i++) {
		int j;

		/* Create it with an index */
		cl_git_pass(git_index_new(&idx));

		base_tree = NULL;
		if (i == 1) {
			cl_git_pass(git_tree_lookup(&base_tree, g_repo, &base_id));
			cl_git_pass(git_index_read_tree(idx, base_tree));
		}

		for (j = 0; j < 3; j++) {
			entry.path = paths[j];
			cl_git_pass(git_index_add(idx, &entry));
		}

		cl_git_pass(git_index_write_tree_to(&tree_index_id, idx, g_repo));
		git_index_free(idx);

		/* Perform the same operations via the tree updater */
		cl_git_pass(git_tree_create_updated(&tree_updater_id, g_repo, base_tree, 3, updates));

		cl_assert_equal_oid(&tree_index_id, &tree_updater_id);
	}
}

void test_object_tree_update__add_conflict(void)
{
	int i;
	git_oid tree_updater_id;
	git_tree_update updates[] = {
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_BLOB, "a/dir/blob"},
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_BLOB, "a/dir"},
	};

	for (i = 0; i < 2; i++) {
		cl_git_pass(git_oid_fromstr(&updates[i].id, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));
	}

	cl_git_fail(git_tree_create_updated(&tree_updater_id, g_repo, NULL, 2, updates));
}

void test_object_tree_update__add_conflict2(void)
{
	int i;
	git_oid tree_updater_id;
	git_tree_update updates[] = {
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_BLOB, "a/dir/blob"},
		{ GIT_TREE_UPDATE_UPSERT, {{0}}, GIT_FILEMODE_TREE, "a/dir/blob"},
	};

	for (i = 0; i < 2; i++) {
		cl_git_pass(git_oid_fromstr(&updates[i].id, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));
	}

	cl_git_fail(git_tree_create_updated(&tree_updater_id, g_repo, NULL, 2, updates));
}
