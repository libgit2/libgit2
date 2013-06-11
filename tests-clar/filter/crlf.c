#include "clar_libgit2.h"
#include "posix.h"
#include "blob.h"
#include "filter.h"
#include "buf_text.h"
#include "git2/filter.h"
#include "crlf_data.h"

static git_repository *g_repo = NULL;

void test_filter_crlf__initialize(void)
{
	git_config *cfg;

	cl_fixture_sandbox("empty_standard_repo");
	cl_git_pass(cl_rename("empty_standard_repo/.gitted", "empty_standard_repo/.git"));

	cl_git_pass(git_repository_open(&g_repo, "empty_standard_repo"));
	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_assert(cfg);

	git_attr_cache_flush(g_repo);
	cl_git_append2file("empty_standard_repo/.gitattributes", "*.txt text\n");
}

void test_filter_crlf__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup("empty_standard_repo");
}

void test_filter_crlf__to_odb(void)
{
	git_filter *filter;
	char *dst = NULL;
	int i, ret;
	size_t dst_size;

	cl_git_pass(git_filter_create__crlf_filter(&filter));

	for (i = 0; i < NUM_CRLF_TEST_OBJECTS; i++) {
		ret = filter->apply_to_odb(filter, g_repo, "filename.txt",
			g_raw[i], g_len_raw[i], &dst, &dst_size);

		cl_assert_equal_i(g_to_odb_expected_ret[i], ret);

		if (!ret) {
			cl_assert_equal_s(g_crlf_filtered_to_odb[i], dst);
			cl_assert_equal_sz(g_len_crlf_filtered_to_odb[i], dst_size);
		}

		if (dst) {
			git__free(dst);
			dst = NULL;
		}
	}

	git__free(filter);
}

void test_filter_crlf__to_worktree(void)
{
	git_filter *filter;
	char *dst = NULL;
	int i, ret;
	size_t dst_size;

	cl_git_pass(git_filter_create__crlf_filter(&filter));

	for (i = 0; i < NUM_CRLF_TEST_OBJECTS; i++) {
		ret = filter->apply_to_worktree(filter, g_repo, "filename.txt",
			g_raw[i], g_len_raw[i], &dst, &dst_size);

		cl_assert_equal_i(g_to_worktree_expected_ret[i], ret);

		if (!ret) {
			cl_assert_equal_s(g_crlf_filtered_to_worktree[i], dst);
			cl_assert_equal_sz(g_len_crlf_filtered_to_worktree[i], dst_size);
		}

		if (dst) {
			git__free(dst);
			dst = NULL;
		}
	}

	git__free(filter);
}
