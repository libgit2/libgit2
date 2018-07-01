#include "../merge/merge_helpers.h"

#define TEST_REPO_PATH "merge-recursive"

#define DIFF_MODIFY_TWO_FILES \
	"diff --git a/asparagus.txt b/asparagus.txt\n" \
	"index f516580..ffb36e5 100644\n" \
	"--- a/asparagus.txt\n" \
	"+++ b/asparagus.txt\n" \
	"@@ -1 +1 @@\n" \
	"-ASPARAGUS SOUP!\n" \
	"+ASPARAGUS SOUP.\n" \
	"diff --git a/veal.txt b/veal.txt\n" \
	"index 94d2c01..a7b0665 100644\n" \
	"--- a/veal.txt\n" \
	"+++ b/veal.txt\n" \
	"@@ -1 +1 @@\n" \
	"-VEAL SOUP!\n" \
	"+VEAL SOUP.\n" \
	"@@ -7 +7 @@ occasionally, then put into it a shin of veal, let it boil two hours\n" \
	"-longer. take out the slices of ham, and skim off the grease if any\n" \
	"+longer; take out the slices of ham, and skim off the grease if any\n"

#define DIFF_DELETE_FILE \
	"diff --git a/gravy.txt b/gravy.txt\n" \
	"deleted file mode 100644\n" \
	"index c4e6cca..0000000\n" \
	"--- a/gravy.txt\n" \
	"+++ /dev/null\n" \
	"@@ -1,8 +0,0 @@\n" \
	"-GRAVY SOUP.\n" \
	"-\n" \
	"-Get eight pounds of coarse lean beef--wash it clean and lay it in your\n" \
	"-pot, put in the same ingredients as for the shin soup, with the same\n" \
	"-quantity of water, and follow the process directed for that. Strain the\n" \
	"-soup through a sieve, and serve it up clear, with nothing more than\n" \
	"-toasted bread in it; two table-spoonsful of mushroom catsup will add a\n" \
	"-fine flavour to the soup.\n"

#define DIFF_ADD_FILE \
	"diff --git a/newfile.txt b/newfile.txt\n" \
	"new file mode 100644\n" \
	"index 0000000..6370543\n" \
	"--- /dev/null\n" \
	"+++ b/newfile.txt\n" \
	"@@ -0,0 +1,2 @@\n" \
	"+This is a new file!\n" \
	"+Added by a patch.\n"

#define DIFF_EXECUTABLE_FILE \
	"diff --git a/beef.txt b/beef.txt\n" \
	"old mode 100644\n" \
	"new mode 100755\n"

struct iterator_compare_data {
	struct merge_index_entry *expected;
	size_t cnt;
	size_t idx;
};

static int iterator_compare(const git_index_entry *entry, void *_data)
{
	git_oid expected_id;

	struct iterator_compare_data *data = (struct iterator_compare_data *)_data;

	cl_assert_equal_i(GIT_IDXENTRY_STAGE(entry), data->expected[data->idx].stage);
	cl_git_pass(git_oid_fromstr(&expected_id, data->expected[data->idx].oid_str));
	cl_assert_equal_oid(&entry->id, &expected_id);
	cl_assert_equal_i(entry->mode, data->expected[data->idx].mode);
	cl_assert_equal_s(entry->path, data->expected[data->idx].path);

	if (data->idx >= data->cnt)
		return -1;

	data->idx++;

	return 0;
}

static void validate_apply_workdir(
	git_repository *repo,
	struct merge_index_entry *workdir_entries,
	size_t workdir_cnt)
{
	git_index *index;
	git_iterator *iterator;
	git_iterator_options opts = GIT_ITERATOR_OPTIONS_INIT;
	struct iterator_compare_data data = { workdir_entries, workdir_cnt };

	opts.flags |= GIT_ITERATOR_INCLUDE_HASH;

	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_iterator_for_workdir(&iterator, repo, index, NULL, &opts));

	cl_git_pass(git_iterator_foreach(iterator, iterator_compare, &data));
	cl_assert_equal_i(data.idx, data.cnt);

	git_iterator_free(iterator);
	git_index_free(index);
}

static void validate_apply_index(
	git_repository *repo,
	struct merge_index_entry *index_entries,
	size_t index_cnt)
{
	git_index *index;
	git_iterator *iterator;
	struct iterator_compare_data data = { index_entries, index_cnt };

	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_iterator_for_index(&iterator, repo, index, NULL));

	cl_git_pass(git_iterator_foreach(iterator, iterator_compare, &data));
	cl_assert_equal_i(data.idx, data.cnt);

	git_iterator_free(iterator);
	git_index_free(index);
}

static int iterator_eq(const git_index_entry **entry, void *_data)
{
	GIT_UNUSED(_data);

	if (!entry[0] || !entry[1])
		return -1;

	cl_assert_equal_i(GIT_IDXENTRY_STAGE(entry[0]), GIT_IDXENTRY_STAGE(entry[1]));
	cl_assert_equal_oid(&entry[0]->id, &entry[1]->id);
	cl_assert_equal_i(entry[0]->mode, entry[1]->mode);
	cl_assert_equal_s(entry[0]->path, entry[1]->path);

	return 0;
}

static void validate_index_unchanged(git_repository *repo)
{
	git_tree *head;
	git_index *index;
	git_iterator *head_iterator, *index_iterator, *iterators[2];

	cl_git_pass(git_repository_head_tree(&head, repo));
	cl_git_pass(git_repository_index(&index, repo));

	cl_git_pass(git_iterator_for_tree(&head_iterator, head, NULL));
	cl_git_pass(git_iterator_for_index(&index_iterator, repo, index, NULL));

	iterators[0] = head_iterator;
	iterators[1] = index_iterator;

	cl_git_pass(git_iterator_walk(iterators, 2, iterator_eq, NULL));

	git_iterator_free(head_iterator);
	git_iterator_free(index_iterator);

	git_tree_free(head);
	git_index_free(index);
}

static void validate_workdir_unchanged(git_repository *repo)
{
	git_tree *head;
	git_index *index;
	git_iterator *head_iterator, *workdir_iterator, *iterators[2];
	git_iterator_options workdir_opts = GIT_ITERATOR_OPTIONS_INIT;

	cl_git_pass(git_repository_head_tree(&head, repo));
	cl_git_pass(git_repository_index(&index, repo));

	workdir_opts.flags |= GIT_ITERATOR_INCLUDE_HASH;

	cl_git_pass(git_iterator_for_tree(&head_iterator, head, NULL));
	cl_git_pass(git_iterator_for_workdir(&workdir_iterator, repo, index, NULL, &workdir_opts));

	iterators[0] = head_iterator;
	iterators[1] = workdir_iterator;

	cl_git_pass(git_iterator_walk(iterators, 2, iterator_eq, NULL));

	git_iterator_free(head_iterator);
	git_iterator_free(workdir_iterator);

	git_tree_free(head);
	git_index_free(index);
}
