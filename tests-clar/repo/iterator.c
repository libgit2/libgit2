#include "clar_libgit2.h"
#include "iterator.h"
#include "repository.h"

static git_repository *g_repo;

void test_repo_iterator__initialize(void)
{
}

void test_repo_iterator__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

static void expect_iterator_items(
	git_iterator *i,
	int expected_flat,
	const char **expected_flat_paths,
	int expected_total,
	const char **expected_total_paths)
{
	const git_index_entry *entry;
	int count;
	int no_trees = !(git_iterator_flags(i) & GIT_ITERATOR_INCLUDE_TREES);

	count = 0;
	cl_git_pass(git_iterator_current(&entry, i));

	while (entry != NULL) {
		if (no_trees)
			cl_assert(entry->mode != GIT_FILEMODE_TREE);

		if (expected_flat_paths) {
			const char *expect_path = expected_flat_paths[count];
			size_t expect_len = strlen(expect_path);

			cl_assert_equal_s(expect_path, entry->path);

			if (expect_path[expect_len - 1] == '/')
				cl_assert_equal_i(GIT_FILEMODE_TREE, entry->mode);
			else
				cl_assert(entry->mode != GIT_FILEMODE_TREE);
		}

		cl_git_pass(git_iterator_advance(&entry, i));

		if (++count > expected_flat)
			break;
	}

	cl_assert_equal_i(expected_flat, count);

	cl_git_pass(git_iterator_reset(i, NULL, NULL));

	count = 0;
	cl_git_pass(git_iterator_current(&entry, i));

	while (entry != NULL) {
		if (no_trees)
			cl_assert(entry->mode != GIT_FILEMODE_TREE);

		if (expected_total_paths) {
			const char *expect_path = expected_total_paths[count];
			size_t expect_len = strlen(expect_path);

			cl_assert_equal_s(expect_path, entry->path);

			if (expect_path[expect_len - 1] == '/')
				cl_assert_equal_i(GIT_FILEMODE_TREE, entry->mode);
			else
				cl_assert(entry->mode != GIT_FILEMODE_TREE);
		}

		if (entry->mode == GIT_FILEMODE_TREE)
			cl_git_pass(git_iterator_advance_into(&entry, i));
		else
			cl_git_pass(git_iterator_advance(&entry, i));

		if (++count > expected_total)
			break;
	}

	cl_assert_equal_i(expected_total, count);
}

/* Index contents (including pseudotrees):
 *
 * 0: a     5: F     10: k/      16: L/
 * 1: B     6: g     11: k/1     17: L/1
 * 2: c     7: H     12: k/a     18: L/a
 * 3: D     8: i     13: k/B     19: L/B
 * 4: e     9: J     14: k/c     20: L/c
 *                   15: k/D     21: L/D
 *
 * 0: B     5: L/    11: a       16: k/
 * 1: D     6: L/1   12: c       17: k/1
 * 2: F     7: L/B   13: e       18: k/B
 * 3: H     8: L/D   14: g       19: k/D
 * 4: J     9: L/a   15: i       20: k/a
 *         10: L/c               21: k/c
 */

void test_repo_iterator__index(void)
{
	git_iterator *i;
	git_index *index;

	g_repo = cl_git_sandbox_init("icase");

	cl_git_pass(git_repository_index(&index, g_repo));

	/* autoexpand with no tree entries for index */
	cl_git_pass(git_iterator_for_index(&i, index, 0, NULL, NULL));
	expect_iterator_items(i, 20, NULL, 20, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_INCLUDE_TREES, NULL, NULL));
	expect_iterator_items(i, 22, NULL, 22, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_DONT_AUTOEXPAND, NULL, NULL));
	expect_iterator_items(i, 12, NULL, 22, NULL);
	git_iterator_free(i);

	git_index_free(index);
}

void test_repo_iterator__index_icase(void)
{
	git_iterator *i;
	git_index *index;
	unsigned int caps;

	g_repo = cl_git_sandbox_init("icase");

	cl_git_pass(git_repository_index(&index, g_repo));
	caps = git_index_caps(index);

	/* force case sensitivity */
	cl_git_pass(git_index_set_caps(index, caps & ~GIT_INDEXCAP_IGNORE_CASE));

	/* autoexpand with no tree entries over range */
	cl_git_pass(git_iterator_for_index(&i, index, 0, "c", "k/D"));
	expect_iterator_items(i, 7, NULL, 7, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_index(&i, index, 0, "k", "k/Z"));
	expect_iterator_items(i, 3, NULL, 3, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_INCLUDE_TREES, "c", "k/D"));
	expect_iterator_items(i, 8, NULL, 8, NULL);
	git_iterator_free(i);
	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_INCLUDE_TREES, "k", "k/Z"));
	expect_iterator_items(i, 4, NULL, 4, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_DONT_AUTOEXPAND, "c", "k/D"));
	expect_iterator_items(i, 5, NULL, 8, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_DONT_AUTOEXPAND, "k", "k/Z"));
	expect_iterator_items(i, 1, NULL, 4, NULL);
	git_iterator_free(i);

	/* force case insensitivity */
	cl_git_pass(git_index_set_caps(index, caps | GIT_INDEXCAP_IGNORE_CASE));

	/* autoexpand with no tree entries over range */
	cl_git_pass(git_iterator_for_index(&i, index, 0, "c", "k/D"));
	expect_iterator_items(i, 13, NULL, 13, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_index(&i, index, 0, "k", "k/Z"));
	expect_iterator_items(i, 5, NULL, 5, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_INCLUDE_TREES, "c", "k/D"));
	expect_iterator_items(i, 14, NULL, 14, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_INCLUDE_TREES, "k", "k/Z"));
	expect_iterator_items(i, 6, NULL, 6, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_DONT_AUTOEXPAND, "c", "k/D"));
	expect_iterator_items(i, 9, NULL, 14, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_index(
		&i, index, GIT_ITERATOR_DONT_AUTOEXPAND, "k", "k/Z"));
	expect_iterator_items(i, 1, NULL, 6, NULL);
	git_iterator_free(i);

	cl_git_pass(git_index_set_caps(index, caps));
	git_index_free(index);
}

void test_repo_iterator__tree(void)
{
	git_iterator *i;
	git_tree *head;

	g_repo = cl_git_sandbox_init("icase");

	cl_git_pass(git_repository_head_tree(&head, g_repo));

	/* auto expand with no tree entries */
	cl_git_pass(git_iterator_for_tree(&i, head, 0, NULL, NULL));
	expect_iterator_items(i, 20, NULL, 20, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_tree(
		&i, head, GIT_ITERATOR_INCLUDE_TREES, NULL, NULL));
	expect_iterator_items(i, 22, NULL, 22, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_tree(
		&i, head, GIT_ITERATOR_DONT_AUTOEXPAND, NULL, NULL));
	expect_iterator_items(i, 12, NULL, 22, NULL);
	git_iterator_free(i);

	git_tree_free(head);
}

void test_repo_iterator__tree_icase(void)
{
	git_iterator *i;
	git_tree *head;
	git_iterator_flag_t flag;

	g_repo = cl_git_sandbox_init("icase");

	cl_git_pass(git_repository_head_tree(&head, g_repo));

	flag = GIT_ITERATOR_DONT_IGNORE_CASE;

	/* auto expand with no tree entries */
	cl_git_pass(git_iterator_for_tree(&i, head, flag, "c", "k/D"));
	expect_iterator_items(i, 7, NULL, 7, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_tree(&i, head, flag, "k", "k/Z"));
	expect_iterator_items(i, 3, NULL, 3, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_INCLUDE_TREES, "c", "k/D"));
	expect_iterator_items(i, 8, NULL, 8, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_INCLUDE_TREES, "k", "k/Z"));
	expect_iterator_items(i, 4, NULL, 4, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "c", "k/D"));
	expect_iterator_items(i, 5, NULL, 8, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "k", "k/Z"));
	expect_iterator_items(i, 1, NULL, 4, NULL);
	git_iterator_free(i);

	flag = GIT_ITERATOR_IGNORE_CASE;

	/* auto expand with no tree entries */
	cl_git_pass(git_iterator_for_tree(&i, head, flag, "c", "k/D"));
	expect_iterator_items(i, 13, NULL, 13, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_tree(&i, head, flag, "k", "k/Z"));
	expect_iterator_items(i, 5, NULL, 5, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_INCLUDE_TREES, "c", "k/D"));
	expect_iterator_items(i, 14, NULL, 14, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_INCLUDE_TREES, "k", "k/Z"));
	expect_iterator_items(i, 6, NULL, 6, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "c", "k/D"));
	expect_iterator_items(i, 9, NULL, 14, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_tree(
		&i, head, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "k", "k/Z"));
	expect_iterator_items(i, 1, NULL, 6, NULL);
	git_iterator_free(i);
}

void test_repo_iterator__tree_more(void)
{
	git_iterator *i;
	git_tree *head;
	static const char *expect_basic[] = {
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
		NULL,
	};
	static const char *expect_trees[] = {
		"current_file",
		"file_deleted",
		"modified_file",
		"staged_changes",
		"staged_changes_file_deleted",
		"staged_changes_modified_file",
		"staged_delete_file_deleted",
		"staged_delete_modified_file",
		"subdir.txt",
		"subdir/",
		"subdir/current_file",
		"subdir/deleted_file",
		"subdir/modified_file",
		NULL,
	};
	static const char *expect_noauto[] = {
		"current_file",
		"file_deleted",
		"modified_file",
		"staged_changes",
		"staged_changes_file_deleted",
		"staged_changes_modified_file",
		"staged_delete_file_deleted",
		"staged_delete_modified_file",
		"subdir.txt",
		"subdir/",
		NULL
	};

	g_repo = cl_git_sandbox_init("status");

	cl_git_pass(git_repository_head_tree(&head, g_repo));

	/* auto expand with no tree entries */
	cl_git_pass(git_iterator_for_tree(&i, head, 0, NULL, NULL));
	expect_iterator_items(i, 12, expect_basic, 12, expect_basic);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_tree(
		&i, head, GIT_ITERATOR_INCLUDE_TREES, NULL, NULL));
	expect_iterator_items(i, 13, expect_trees, 13, expect_trees);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_tree(
		&i, head, GIT_ITERATOR_DONT_AUTOEXPAND, NULL, NULL));
	expect_iterator_items(i, 10, expect_noauto, 13, expect_trees);
	git_iterator_free(i);

	git_tree_free(head);
}

void test_repo_iterator__workdir(void)
{
	git_iterator *i;

	g_repo = cl_git_sandbox_init("icase");

	/* auto expand with no tree entries */
	cl_git_pass(git_iterator_for_workdir(&i, g_repo, 0, NULL, NULL));
	expect_iterator_items(i, 20, NULL, 20, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, GIT_ITERATOR_INCLUDE_TREES, NULL, NULL));
	expect_iterator_items(i, 22, NULL, 22, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, GIT_ITERATOR_DONT_AUTOEXPAND, NULL, NULL));
	expect_iterator_items(i, 12, NULL, 22, NULL);
	git_iterator_free(i);
}

void test_repo_iterator__workdir_icase(void)
{
	git_iterator *i;
	git_iterator_flag_t flag;

	g_repo = cl_git_sandbox_init("icase");

	flag = GIT_ITERATOR_DONT_IGNORE_CASE;

	/* auto expand with no tree entries */
	cl_git_pass(git_iterator_for_workdir(&i, g_repo, flag, "c", "k/D"));
	expect_iterator_items(i, 7, NULL, 7, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_workdir(&i, g_repo, flag, "k", "k/Z"));
	expect_iterator_items(i, 3, NULL, 3, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_INCLUDE_TREES, "c", "k/D"));
	expect_iterator_items(i, 8, NULL, 8, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_INCLUDE_TREES, "k", "k/Z"));
	expect_iterator_items(i, 4, NULL, 4, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "c", "k/D"));
	expect_iterator_items(i, 5, NULL, 8, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "k", "k/Z"));
	expect_iterator_items(i, 1, NULL, 4, NULL);
	git_iterator_free(i);

	flag = GIT_ITERATOR_IGNORE_CASE;

	/* auto expand with no tree entries */
	cl_git_pass(git_iterator_for_workdir(&i, g_repo, flag, "c", "k/D"));
	expect_iterator_items(i, 13, NULL, 13, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_workdir(&i, g_repo, flag, "k", "k/Z"));
	expect_iterator_items(i, 5, NULL, 5, NULL);
	git_iterator_free(i);

	/* auto expand with tree entries */
	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_INCLUDE_TREES, "c", "k/D"));
	expect_iterator_items(i, 14, NULL, 14, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_INCLUDE_TREES, "k", "k/Z"));
	expect_iterator_items(i, 6, NULL, 6, NULL);
	git_iterator_free(i);

	/* no auto expand (implies trees included) */
	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "c", "k/D"));
	expect_iterator_items(i, 9, NULL, 14, NULL);
	git_iterator_free(i);

	cl_git_pass(git_iterator_for_workdir(
		&i, g_repo, flag | GIT_ITERATOR_DONT_AUTOEXPAND, "k", "k/Z"));
	expect_iterator_items(i, 1, NULL, 6, NULL);
	git_iterator_free(i);
}
