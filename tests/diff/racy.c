#include "clar_libgit2.h"
#include "../checkout/checkout_helpers.h"

#include "buffer.h"

static git_repository *g_repo;

void test_diff_racy__initialize(void)
{
	cl_git_pass(git_repository_init(&g_repo, "diff_racy", false));
}

void test_diff_racy__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;

	cl_fixture_cleanup("diff_racy");
}

void test_diff_racy__diff(void)
{
	git_index *index;
	git_diff *diff;
	git_buf path = GIT_BUF_INIT;

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "A"));
	cl_git_mkfile(path.ptr, "A");

	/* Put 'A' into the index */
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "A"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, index, NULL));
	cl_assert_equal_i(0, git_diff_num_deltas(diff));
	git_diff_free(diff);

	/* Change its contents quickly, so we get the same timestamp */
	cl_git_mkfile(path.ptr, "B");

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, index, NULL));
	cl_assert_equal_i(1, git_diff_num_deltas(diff));

	git_index_free(index);
	git_diff_free(diff);
	git_buf_free(&path);
}

void test_diff_racy__write_index_just_after_file(void)
{
	git_index *index;
	git_diff *diff;
	git_buf path = GIT_BUF_INIT;

	/* Make sure we do have a timestamp */
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_write(index));
	/* The timestamp will be one second before we change the file */
	sleep(1);

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "A"));
	cl_git_mkfile(path.ptr, "A");

	/*
	 * Put 'A' into the index, the size field will be filled,
	 * because the index' on-disk timestamp does not match the
	 * file's timestamp. Both timestamps will however match after
	 * writing out the index.
	 */
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "A"));
	cl_git_pass(git_index_write(index));

	/* Change its contents quickly, so we get the same timestamp */
	cl_git_mkfile(path.ptr, "B");

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, index, NULL));
	cl_assert_equal_i(1, git_diff_num_deltas(diff));
}
