#include "clar_libgit2.h"
#include "diff_helpers.h"
#include "iterator.h"

void test_diff_iterator__initialize(void)
{
	/* since we are doing tests with different sandboxes, defer setup
	 * to the actual tests.  cleanup will still be done in the global
	 * cleanup function so that assertion failures don't result in a
	 * missed cleanup.
	 */
}

void test_diff_iterator__cleanup(void)
{
	cl_git_sandbox_cleanup();
}


/* -- TREE ITERATOR TESTS -- */

static void tree_iterator_test(
	const char *sandbox,
	const char *treeish,
	int expected_count,
	const char **expected_values)
{
	git_tree *t;
	git_iterator *i;
	const git_index_entry *entry;
	int count = 0;
	git_repository *repo = cl_git_sandbox_init(sandbox);

	cl_assert(t = resolve_commit_oid_to_tree(repo, treeish));
	cl_git_pass(git_iterator_for_tree(repo, t, &i));
	cl_git_pass(git_iterator_current(i, &entry));

	while (entry != NULL) {
		if (expected_values != NULL)
			cl_assert_strequal(expected_values[count], entry->path);

		count++;

		cl_git_pass(git_iterator_advance(i, &entry));
	}

	git_iterator_free(i);

	cl_assert(expected_count == count);

	git_tree_free(t);
}

/* results of: git ls-tree -r --name-only 605812a */
const char *expected_tree_0[] = {
	".gitattributes",
	"attr0",
	"attr1",
	"attr2",
	"attr3",
	"binfile",
	"macro_test",
	"root_test1",
	"root_test2",
	"root_test3",
	"root_test4.txt",
	"subdir/.gitattributes",
	"subdir/abc",
	"subdir/subdir_test1",
	"subdir/subdir_test2.txt",
	"subdir2/subdir2_test1",
	NULL
};

void test_diff_iterator__tree_0(void)
{
	tree_iterator_test("attr", "605812a", 16, expected_tree_0);
}

/* results of: git ls-tree -r --name-only 6bab5c79 */
const char *expected_tree_1[] = {
	".gitattributes",
	"attr0",
	"attr1",
	"attr2",
	"attr3",
	"root_test1",
	"root_test2",
	"root_test3",
	"root_test4.txt",
	"subdir/.gitattributes",
	"subdir/subdir_test1",
	"subdir/subdir_test2.txt",
	"subdir2/subdir2_test1",
	NULL
};

void test_diff_iterator__tree_1(void)
{
	tree_iterator_test("attr", "6bab5c79cd5", 13, expected_tree_1);
}

/* results of: git ls-tree -r --name-only 26a125ee1 */
const char *expected_tree_2[] = {
	"current_file",
	"file_deleted",
	"modified_file",
	"staged_changes",
	"staged_changes_file_deleted",
	"staged_changes_modified_file",
	"staged_delete_file_deleted",
	"staged_delete_modified_file",
	"subdir.txt",
	"subdir/current_file",
	"subdir/deleted_file",
	"subdir/modified_file",
	NULL
};

void test_diff_iterator__tree_2(void)
{
	tree_iterator_test("status", "26a125ee1", 12, expected_tree_2);
}

/* $ git ls-tree -r --name-only 0017bd4ab1e */
const char *expected_tree_3[] = {
	"current_file",
	"file_deleted",
	"modified_file",
	"staged_changes",
	"staged_changes_file_deleted",
	"staged_changes_modified_file",
	"staged_delete_file_deleted",
	"staged_delete_modified_file"
};

void test_diff_iterator__tree_3(void)
{
	tree_iterator_test("status", "0017bd4ab1e", 8, expected_tree_3);
}


/* -- INDEX ITERATOR TESTS -- */

static void index_iterator_test(
	const char *sandbox,
	int expected_count,
	const char **expected_names,
	const char **expected_oids)
{
	git_iterator *i;
	const git_index_entry *entry;
	int count = 0;
	git_repository *repo = cl_git_sandbox_init(sandbox);

	cl_git_pass(git_iterator_for_index(repo, &i));
	cl_git_pass(git_iterator_current(i, &entry));

	while (entry != NULL) {
		if (expected_names != NULL)
			cl_assert_strequal(expected_names[count], entry->path);

		if (expected_oids != NULL) {
			git_oid oid;
			cl_git_pass(git_oid_fromstr(&oid, expected_oids[count]));
			cl_assert(git_oid_cmp(&oid, &entry->oid) == 0);
		}

		count++;
		cl_git_pass(git_iterator_advance(i, &entry));
	}

	git_iterator_free(i);

	cl_assert(count == expected_count);
}

static const char *expected_index_0[] = {
	"attr0",
	"attr1",
	"attr2",
	"attr3",
	"binfile",
	"gitattributes",
	"macro_bad",
	"macro_test",
	"root_test1",
	"root_test2",
	"root_test3",
	"root_test4.txt",
	"subdir/.gitattributes",
	"subdir/abc",
	"subdir/subdir_test1",
	"subdir/subdir_test2.txt",
	"subdir2/subdir2_test1",
};

static const char *expected_index_oids_0[] = {
	"556f8c827b8e4a02ad5cab77dca2bcb3e226b0b3",
	"3b74db7ab381105dc0d28f8295a77f6a82989292",
	"2c66e14f77196ea763fb1e41612c1aa2bc2d8ed2",
	"c485abe35abd4aa6fd83b076a78bbea9e2e7e06c",
	"d800886d9c86731ae5c4a62b0b77c437015e00d2",
	"2b40c5aca159b04ea8d20ffe36cdf8b09369b14a",
	"5819a185d77b03325aaf87cafc771db36f6ddca7",
	"ff69f8639ce2e6010b3f33a74160aad98b48da2b",
	"45141a79a77842c59a63229403220a4e4be74e3d",
	"4d713dc48e6b1bd75b0d61ad078ba9ca3a56745d",
	"108bb4e7fd7b16490dc33ff7d972151e73d7166e",
	"fe773770c5a6cc7185580c9204b1ff18a33ff3fc",
	"99eae476896f4907224978b88e5ecaa6c5bb67a9",
	"3e42ffc54a663f9401cc25843d6c0e71a33e4249",
	"e563cf4758f0d646f1b14b76016aa17fa9e549a4",
	"fb5067b1aef3ac1ada4b379dbcb7d17255df7d78",
	"dccada462d3df8ac6de596fb8c896aba9344f941"
};

void test_diff_iterator__index_0(void)
{
	index_iterator_test("attr", 17, expected_index_0, expected_index_oids_0);
}

static const char *expected_index_1[] = {
	"current_file",
	"file_deleted",
	"modified_file",
	"staged_changes",
	"staged_changes_file_deleted",
	"staged_changes_modified_file",
	"staged_new_file",
	"staged_new_file_deleted_file",
	"staged_new_file_modified_file",
	"subdir.txt",
	"subdir/current_file",
	"subdir/deleted_file",
	"subdir/modified_file",
};

static const char* expected_index_oids_1[] = {
	"a0de7e0ac200c489c41c59dfa910154a70264e6e",
	"5452d32f1dd538eb0405e8a83cc185f79e25e80f",
	"452e4244b5d083ddf0460acf1ecc74db9dcfa11a",
	"55d316c9ba708999f1918e9677d01dfcae69c6b9",
	"a6be623522ce87a1d862128ac42672604f7b468b",
	"906ee7711f4f4928ddcb2a5f8fbc500deba0d2a8",
	"529a16e8e762d4acb7b9636ff540a00831f9155a",
	"90b8c29d8ba39434d1c63e1b093daaa26e5bd972",
	"ed062903b8f6f3dccb2fa81117ba6590944ef9bd",
	"e8ee89e15bbe9b20137715232387b3de5b28972e",
	"53ace0d1cc1145a5f4fe4f78a186a60263190733",
	"1888c805345ba265b0ee9449b8877b6064592058",
	"a6191982709b746d5650e93c2acf34ef74e11504"
};

void test_diff_iterator__index_1(void)
{
	index_iterator_test("status", 13, expected_index_1, expected_index_oids_1);
}


/* -- WORKDIR ITERATOR TESTS -- */

static void workdir_iterator_test(
	const char *sandbox,
	int expected_count,
	int expected_ignores,
	const char **expected_names,
	const char *an_ignored_name)
{
	git_iterator *i;
	const git_index_entry *entry;
	int count = 0, count_all = 0;
	git_repository *repo = cl_git_sandbox_init(sandbox);

	cl_git_pass(git_iterator_for_workdir(repo, &i));
	cl_git_pass(git_iterator_current(i, &entry));

	while (entry != NULL) {
		int ignored = git_iterator_current_is_ignored(i);

		if (S_ISDIR(entry->mode)) {
			cl_git_pass(git_iterator_advance_into_directory(i, &entry));
			continue;
		}

		if (expected_names != NULL)
			cl_assert_strequal(expected_names[count_all], entry->path);

		if (an_ignored_name && strcmp(an_ignored_name,entry->path)==0)
			cl_assert(ignored);

		if (!ignored)
			count++;
		count_all++;

		cl_git_pass(git_iterator_advance(i, &entry));
	}

	git_iterator_free(i);

	cl_assert(count == expected_count);
	cl_assert(count_all == expected_count + expected_ignores);
}

void test_diff_iterator__workdir_0(void)
{
	workdir_iterator_test("attr", 25, 2, NULL, "ign");
}

static const char *status_paths[] = {
	"current_file",
	"ignored_file",
	"modified_file",
	"new_file",
	"staged_changes",
	"staged_changes_modified_file",
	"staged_delete_modified_file",
	"staged_new_file",
	"staged_new_file_modified_file",
	"subdir.txt",
	"subdir/current_file",
	"subdir/modified_file",
	"subdir/new_file",
	NULL
};

void test_diff_iterator__workdir_1(void)
{
	workdir_iterator_test("status", 12, 1, status_paths, "ignored_file");
}
