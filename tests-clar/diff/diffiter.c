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
	size_t d, num_d;

	cl_git_pass(git_diff_workdir_to_index(repo, NULL, &diff));

	num_d = git_diff_num_deltas(diff);
	for (d = 0; d < num_d; ++d) {
		const git_diff_delta *delta;
		cl_git_pass(git_diff_get_patch(NULL, &delta, diff, d));
	}

	git_diff_list_free(diff);
}

void test_diff_diffiter__iterate_files(void)
{
	git_repository *repo = cl_git_sandbox_init("attr");
	git_diff_list *diff;
	size_t d, num_d;
	int count = 0;

	cl_git_pass(git_diff_workdir_to_index(repo, NULL, &diff));

	num_d = git_diff_num_deltas(diff);
	cl_assert_equal_i(6, (int)num_d);

	for (d = 0; d < num_d; ++d) {
		const git_diff_delta *delta;
		cl_git_pass(git_diff_get_patch(NULL, &delta, diff, d));
		cl_assert(delta != NULL);
		count++;
	}
	cl_assert_equal_i(6, count);

	git_diff_list_free(diff);
}

void test_diff_diffiter__iterate_files_2(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_diff_list *diff;
	size_t d, num_d;
	int count = 0;

	cl_git_pass(git_diff_workdir_to_index(repo, NULL, &diff));

	num_d = git_diff_num_deltas(diff);
	cl_assert_equal_i(8, (int)num_d);

	for (d = 0; d < num_d; ++d) {
		const git_diff_delta *delta;
		cl_git_pass(git_diff_get_patch(NULL, &delta, diff, d));
		cl_assert(delta != NULL);
		count++;
	}
	cl_assert_equal_i(8, count);

	git_diff_list_free(diff);
}

void test_diff_diffiter__iterate_files_and_hunks(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	size_t d, num_d;
	int file_count = 0, hunk_count = 0;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;

	cl_git_pass(git_diff_workdir_to_index(repo, &opts, &diff));

	num_d = git_diff_num_deltas(diff);

	for (d = 0; d < num_d; ++d) {
		git_diff_patch *patch;
		const git_diff_delta *delta;
		size_t h, num_h;

		cl_git_pass(git_diff_get_patch(&patch, &delta, diff, d));

		cl_assert(delta);
		cl_assert(patch);

		file_count++;

		num_h = git_diff_patch_num_hunks(patch);

		for (h = 0; h < num_h; h++) {
			const git_diff_range *range;
			const char *header;
			size_t header_len, num_l;

			cl_git_pass(git_diff_patch_get_hunk(
				&range, &header, &header_len, &num_l, patch, h));

			cl_assert(range);
			cl_assert(header);

			hunk_count++;
		}

		git_diff_patch_free(patch);
	}

	cl_assert_equal_i(13, file_count);
	cl_assert_equal_i(8, hunk_count);

	git_diff_list_free(diff);
}

void test_diff_diffiter__max_size_threshold(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	int file_count = 0, binary_count = 0, hunk_count = 0;
	size_t d, num_d;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;

	cl_git_pass(git_diff_workdir_to_index(repo, &opts, &diff));
	num_d = git_diff_num_deltas(diff);

	for (d = 0; d < num_d; ++d) {
		git_diff_patch *patch;
		const git_diff_delta *delta;

		cl_git_pass(git_diff_get_patch(&patch, &delta, diff, d));
		cl_assert(delta);
		cl_assert(patch);

		file_count++;
		hunk_count += (int)git_diff_patch_num_hunks(patch);

		assert(delta->binary == 0 || delta->binary == 1);
		binary_count += delta->binary;

		git_diff_patch_free(patch);
	}

	cl_assert_equal_i(13, file_count);
	cl_assert_equal_i(0, binary_count);
	cl_assert_equal_i(8, hunk_count);

	git_diff_list_free(diff);

	/* try again with low file size threshold */

	file_count = binary_count = hunk_count = 0;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;
	opts.max_size = 50; /* treat anything over 50 bytes as binary! */

	cl_git_pass(git_diff_workdir_to_index(repo, &opts, &diff));
	num_d = git_diff_num_deltas(diff);

	for (d = 0; d < num_d; ++d) {
		git_diff_patch *patch;
		const git_diff_delta *delta;

		cl_git_pass(git_diff_get_patch(&patch, &delta, diff, d));

		file_count++;
		hunk_count += (int)git_diff_patch_num_hunks(patch);

		assert(delta->binary == 0 || delta->binary == 1);
		binary_count += delta->binary;

		git_diff_patch_free(patch);
	}

	cl_assert_equal_i(13, file_count);
	/* Three files are over the 50 byte threshold:
	 * - staged_changes_file_deleted
	 * - staged_changes_modified_file
	 * - staged_new_file_modified_file
	 */
	cl_assert_equal_i(3, binary_count);
	cl_assert_equal_i(5, hunk_count);

	git_diff_list_free(diff);
}


void test_diff_diffiter__iterate_all(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects exp = {0};
	size_t d, num_d;

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;

	cl_git_pass(git_diff_workdir_to_index(repo, &opts, &diff));

	num_d = git_diff_num_deltas(diff);
	for (d = 0; d < num_d; ++d) {
		git_diff_patch *patch;
		const git_diff_delta *delta;
		size_t h, num_h;

		cl_git_pass(git_diff_get_patch(&patch, &delta, diff, d));
		cl_assert(patch && delta);
		exp.files++;

		num_h = git_diff_patch_num_hunks(patch);
		for (h = 0; h < num_h; h++) {
			const git_diff_range *range;
			const char *header;
			size_t header_len, l, num_l;

			cl_git_pass(git_diff_patch_get_hunk(
				&range, &header, &header_len, &num_l, patch, h));
			cl_assert(range && header);
			exp.hunks++;

			for (l = 0; l < num_l; ++l) {
				char origin;
				const char *content;
				size_t content_len;

				cl_git_pass(git_diff_patch_get_line_in_hunk(
					&origin, &content, &content_len, NULL, NULL, patch, h, l));
				cl_assert(content);
				exp.lines++;
			}
		}

		git_diff_patch_free(patch);
	}

	cl_assert_equal_i(13, exp.files);
	cl_assert_equal_i(8, exp.hunks);
	cl_assert_equal_i(14, exp.lines);

	git_diff_list_free(diff);
}

static void iterate_over_patch(git_diff_patch *patch, diff_expects *exp)
{
	size_t h, num_h = git_diff_patch_num_hunks(patch), num_l;

	exp->files++;
	exp->hunks += (int)num_h;

	/* let's iterate in reverse, just because we can! */
	for (h = 1, num_l = 0; h <= num_h; ++h)
		num_l += git_diff_patch_num_lines_in_hunk(patch, num_h - h);

	exp->lines += (int)num_l;
}

#define PATCH_CACHE 5

void test_diff_diffiter__iterate_randomly_while_saving_state(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_diff_options opts = {0};
	git_diff_list *diff = NULL;
	diff_expects exp = {0};
	git_diff_patch *patches[PATCH_CACHE];
	size_t p, d, num_d;

	memset(patches, 0, sizeof(patches));

	opts.context_lines = 3;
	opts.interhunk_lines = 1;
	opts.flags |= GIT_DIFF_INCLUDE_IGNORED | GIT_DIFF_INCLUDE_UNTRACKED;

	cl_git_pass(git_diff_workdir_to_index(repo, &opts, &diff));

	num_d = git_diff_num_deltas(diff);

	/* To make sure that references counts work for diff and patch objects,
	 * this generates patches and randomly caches them.  Only when the patch
	 * is removed from the cache are hunks and lines counted.  At the end,
	 * there are still patches in the cache, so free the diff and try to
	 * process remaining patches after the diff is freed.
	 */

	srand(121212);
	p = rand() % PATCH_CACHE;

	for (d = 0; d < num_d; ++d) {
		/* take old patch */
		git_diff_patch *patch = patches[p];
		patches[p] = NULL;

		/* cache new patch */
		cl_git_pass(git_diff_get_patch(&patches[p], NULL, diff, d));
		cl_assert(patches[p] != NULL);

		/* process old patch if non-NULL */
		if (patch != NULL) {
			iterate_over_patch(patch, &exp);
			git_diff_patch_free(patch);
		}

		p = rand() % PATCH_CACHE;
	}

	/* free diff list now - refcounts should keep things safe */
	git_diff_list_free(diff);

	/* process remaining unprocessed patches */
	for (p = 0; p < PATCH_CACHE; p++) {
		git_diff_patch *patch = patches[p];

		if (patch != NULL) {
			iterate_over_patch(patch, &exp);
			git_diff_patch_free(patch);
		}
	}

	/* hopefully it all still added up right */
	cl_assert_equal_i(13, exp.files);
	cl_assert_equal_i(8, exp.hunks);
	cl_assert_equal_i(14, exp.lines);
}
