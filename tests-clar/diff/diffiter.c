#include "clar_libgit2.h"
#include "diff_helpers.h"

void test_diff_diffiter__initialize(void)
{
}

void test_diff_diffiter__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_diff_diffiter__create(void)
{
	git_repository *repo = cl_git_sandbox_init("attr");
	git_diff_list *diff;
	git_diff_iterator *iter;

	cl_git_pass(git_diff_workdir_to_index(repo, NULL, &diff));
	cl_git_pass(git_diff_iterator_new(&iter, diff));
	git_diff_iterator_free(iter);
	git_diff_list_free(diff);
}

void test_diff_diffiter__iterate_files(void)
{
	git_repository *repo = cl_git_sandbox_init("attr");
	git_diff_list *diff;
	git_diff_iterator *iter;
	git_diff_delta *delta;
	int error, count = 0;

	cl_git_pass(git_diff_workdir_to_index(repo, NULL, &diff));
	cl_git_pass(git_diff_iterator_new(&iter, diff));

	while ((error = git_diff_iterator_next_file(&delta, iter)) != GIT_ITEROVER) {
		cl_assert_equal_i(0, error);
		cl_assert(delta != NULL);
		count++;
	}

	cl_assert_equal_i(GIT_ITEROVER, error);
	cl_assert(delta == NULL);
	cl_assert_equal_i(6, count);

	git_diff_iterator_free(iter);
	git_diff_list_free(diff);
}

void test_diff_diffiter__iterate_files_2(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_diff_list *diff;
	git_diff_iterator *iter;
	git_diff_delta *delta;
	int error, count = 0;

	cl_git_pass(git_diff_workdir_to_index(repo, NULL, &diff));
	cl_git_pass(git_diff_iterator_new(&iter, diff));

	while ((error = git_diff_iterator_next_file(&delta, iter)) != GIT_ITEROVER) {
		cl_assert_equal_i(0, error);
		cl_assert(delta != NULL);
		count++;
	}

	cl_assert_equal_i(GIT_ITEROVER, error);
	cl_assert(delta == NULL);
	cl_assert_equal_i(8, count);

	git_diff_iterator_free(iter);
	git_diff_list_free(diff);
}

void test_diff_diffiter__iterate_files_and_hunks(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	git_diff_iterator *iter;
	git_diff_delta *delta;
	git_diff_range *range;
	const char *header;
	size_t header_len;
	int error, file_count = 0, hunk_count = 0;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;

	cl_git_pass(git_diff_workdir_to_index(repo, &opts, &diff));

	cl_git_pass(git_diff_iterator_new(&iter, diff));

	while ((error = git_diff_iterator_next_file(&delta, iter)) != GIT_ITEROVER) {
		cl_assert_equal_i(0, error);
		cl_assert(delta);

		file_count++;

		while ((error = git_diff_iterator_next_hunk(
				&range, &header, &header_len, iter)) != GIT_ITEROVER) {
			cl_assert_equal_i(0, error);
			cl_assert(range);
			hunk_count++;
		}
	}

	cl_assert_equal_i(GIT_ITEROVER, error);
	cl_assert(delta == NULL);
	cl_assert_equal_i(13, file_count);
	cl_assert_equal_i(8, hunk_count);

	git_diff_iterator_free(iter);
	git_diff_list_free(diff);
}
