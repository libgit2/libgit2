#include "clar_libgit2.h"
#include "../merge/merge_helpers.h"

static git_repository *repo;

#define TEST_REPO_PATH "merge-recursive"

void test_apply_workdir__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
}

void test_apply_workdir__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

struct iterator_compare_data {
	struct merge_index_entry *expected;
	size_t cnt;
	size_t idx;
};

static int iterator_compare(const git_index_entry **entries, void *_data)
{
	const git_index_entry *head_entry = entries[0];
	const git_index_entry *index_entry = entries[1];
	const git_index_entry *workdir_entry = entries[2];
	git_oid expected_id;
	struct iterator_compare_data *data = (struct iterator_compare_data *)_data;

	if (!head_entry || !index_entry) {
		cl_assert_equal_p(head_entry, index_entry);
	} else {
		cl_assert_equal_i(GIT_IDXENTRY_STAGE(head_entry), GIT_IDXENTRY_STAGE(index_entry));
		cl_assert_equal_oid(&head_entry->id, &index_entry->id);
		cl_assert_equal_i(head_entry->mode, index_entry->mode);
		cl_assert_equal_s(head_entry->path, index_entry->path);
	}

	if (!workdir_entry)
		return 0;

	cl_assert_equal_i(GIT_IDXENTRY_STAGE(workdir_entry), data->expected[data->idx].stage);
	cl_git_pass(git_oid_fromstr(&expected_id, data->expected[data->idx].oid_str));
	cl_assert_equal_oid(&workdir_entry->id, &expected_id);
	cl_assert_equal_i(workdir_entry->mode, data->expected[data->idx].mode);
	cl_assert_equal_s(workdir_entry->path, data->expected[data->idx].path);

	if (data->idx >= data->cnt)
		return -1;

	data->idx++;

	return 0;
}

static void validate_apply_workdir(
	git_repository *repo,
	struct merge_index_entry *entries,
	size_t cnt)
{
	git_tree *head;
	git_index *repo_index;
	git_iterator *head_iterator, *index_iterator, *workdir_iterator, *iterators[3];
	git_iterator_options workdir_opts = GIT_ITERATOR_OPTIONS_INIT;

	struct iterator_compare_data data = { entries, cnt };

	workdir_opts.flags |= GIT_ITERATOR_INCLUDE_HASH;

	cl_git_pass(git_repository_head_tree(&head, repo));
	cl_git_pass(git_repository_index(&repo_index, repo));

	cl_git_pass(git_iterator_for_tree(&head_iterator, head, NULL));
	cl_git_pass(git_iterator_for_index(&index_iterator, repo, repo_index, NULL));
	cl_git_pass(git_iterator_for_workdir(&workdir_iterator, repo, repo_index, NULL, &workdir_opts));

	iterators[0] = head_iterator;
	iterators[1] = index_iterator;
	iterators[2] = workdir_iterator;

	cl_git_pass(git_iterator_walk(iterators, 3, iterator_compare, &data));
	cl_assert_equal_i(data.idx, data.cnt);

	git_iterator_free(head_iterator);
	git_iterator_free(index_iterator);
	git_iterator_free(workdir_iterator);
	git_index_free(repo_index);
	git_tree_free(head);
}

void test_apply_workdir__generated_diff(void)
{
	git_oid a_oid, b_oid;
	git_commit *a_commit, *b_commit;
	git_tree *a_tree, *b_tree;
	git_diff *diff;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;

	struct merge_index_entry expected[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "a7b066537e6be7109abfe4ff97b675d4e077da20", 0, "veal.txt" },
	};
	size_t expected_cnt = sizeof(expected) / sizeof(struct merge_index_entry);

	git_oid_fromstr(&a_oid, "539bd011c4822c560c1d17cab095006b7a10f707");
	git_oid_fromstr(&b_oid, "7c7bf85e978f1d18c0566f702d2cb7766b9c8d4f");

	cl_git_pass(git_commit_lookup(&a_commit, repo, &a_oid));
	cl_git_pass(git_commit_lookup(&b_commit, repo, &b_oid));

	cl_git_pass(git_commit_tree(&a_tree, a_commit));
	cl_git_pass(git_commit_tree(&b_tree, b_commit));

	cl_git_pass(git_diff_tree_to_tree(&diff, repo, a_tree, b_tree, &opts));

    cl_git_pass(git_reset(repo, (git_object *)a_commit, GIT_RESET_HARD, NULL));
	cl_git_pass(git_apply(repo, diff, NULL));

	validate_apply_workdir(repo, expected, expected_cnt);

	git_diff_free(diff);
	git_tree_free(a_tree);
	git_tree_free(b_tree);
	git_commit_free(a_commit);
	git_commit_free(b_commit);
}

void test_apply_workdir__parsed_diff(void)
{
	git_oid oid;
	git_commit *commit;
	git_diff *diff;

	const char *diff_file =
		"diff --git a/asparagus.txt b/asparagus.txt\n"
		"index f516580..ffb36e5 100644\n"
		"--- a/asparagus.txt\n"
		"+++ b/asparagus.txt\n"
		"@@ -1 +1 @@\n"
		"-ASPARAGUS SOUP!\n"
		"+ASPARAGUS SOUP.\n"
		"diff --git a/veal.txt b/veal.txt\n"
		"index 94d2c01..a7b0665 100644\n"
		"--- a/veal.txt\n"
		"+++ b/veal.txt\n"
		"@@ -1 +1 @@\n"
		"-VEAL SOUP!\n"
		"+VEAL SOUP.\n"
		"@@ -7 +7 @@ occasionally, then put into it a shin of veal, let it boil two hours\n"
		"-longer. take out the slices of ham, and skim off the grease if any\n"
		"+longer; take out the slices of ham, and skim off the grease if any\n";

	struct merge_index_entry expected[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "a7b066537e6be7109abfe4ff97b675d4e077da20", 0, "veal.txt" },
	};
	size_t expected_cnt = sizeof(expected) / sizeof(struct merge_index_entry);

	git_oid_fromstr(&oid, "539bd011c4822c560c1d17cab095006b7a10f707");
	cl_git_pass(git_commit_lookup(&commit, repo, &oid));

	cl_git_pass(git_diff_from_buffer(&diff, diff_file, strlen(diff_file)));

	cl_git_pass(git_reset(repo, (git_object *)commit, GIT_RESET_HARD, NULL));
	cl_git_pass(git_apply(repo, diff, NULL));

	validate_apply_workdir(repo, expected, expected_cnt);

	git_diff_free(diff);
	git_commit_free(commit);
}

void test_apply_workdir__removes_file(void)
{
	git_oid oid;
	git_commit *commit;
	git_diff *diff;

	const char *diff_file =
		"diff --git a/gravy.txt b/gravy.txt\n"
		"deleted file mode 100644\n"
		"index c4e6cca..0000000\n"
		"--- a/gravy.txt\n"
		"+++ /dev/null\n"
		"@@ -1,8 +0,0 @@\n"
		"-GRAVY SOUP.\n"
		"-\n"
		"-Get eight pounds of coarse lean beef--wash it clean and lay it in your\n"
		"-pot, put in the same ingredients as for the shin soup, with the same\n"
		"-quantity of water, and follow the process directed for that. Strain the\n"
		"-soup through a sieve, and serve it up clear, with nothing more than\n"
		"-toasted bread in it; two table-spoonsful of mushroom catsup will add a\n"
		"-fine flavour to the soup.\n";

	struct merge_index_entry expected[] = {
		{ 0100644, "f51658077d85f2264fa179b4d0848268cb3475c3", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "94d2c01087f48213bd157222d54edfefd77c9bba", 0, "veal.txt" },
	};
	size_t expected_cnt = sizeof(expected) / sizeof(struct merge_index_entry);

	git_oid_fromstr(&oid, "539bd011c4822c560c1d17cab095006b7a10f707");
	cl_git_pass(git_commit_lookup(&commit, repo, &oid));

	cl_git_pass(git_diff_from_buffer(&diff, diff_file, strlen(diff_file)));

	cl_git_pass(git_reset(repo, (git_object *)commit, GIT_RESET_HARD, NULL));
	cl_git_pass(git_apply(repo, diff, NULL));

	validate_apply_workdir(repo, expected, expected_cnt);

	git_diff_free(diff);
	git_commit_free(commit);
}

void test_apply_workdir__adds_file(void)
{
	git_oid oid;
	git_commit *commit;
	git_diff *diff;

	const char *diff_file =
		"diff --git a/newfile.txt b/newfile.txt\n"
		"new file mode 100644\n"
		"index 0000000..6370543\n"
		"--- /dev/null\n"
		"+++ b/newfile.txt\n"
		"@@ -0,0 +1,2 @@\n"
		"+This is a new file!\n"
		"+Added by a patch.\n";

	struct merge_index_entry expected[] = {
		{ 0100644, "f51658077d85f2264fa179b4d0848268cb3475c3", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "6370543fcfedb3e6516ec53b06158f3687dc1447", 0, "newfile.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "94d2c01087f48213bd157222d54edfefd77c9bba", 0, "veal.txt" },
	};
	size_t expected_cnt = sizeof(expected) / sizeof(struct merge_index_entry);

	git_oid_fromstr(&oid, "539bd011c4822c560c1d17cab095006b7a10f707");
	cl_git_pass(git_commit_lookup(&commit, repo, &oid));

	cl_git_pass(git_diff_from_buffer(&diff, diff_file, strlen(diff_file)));

	cl_git_pass(git_reset(repo, (git_object *)commit, GIT_RESET_HARD, NULL));
	cl_git_pass(git_apply(repo, diff, NULL));

	validate_apply_workdir(repo, expected, expected_cnt);

	git_diff_free(diff);
	git_commit_free(commit);
}
