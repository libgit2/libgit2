#include "clar_libgit2.h"
#include "posix.h"
#include "blob.h"
#include "filter.h"
#include "buf_text.h"
#include "git2/filter.h"

static git_repository *g_repo = NULL;

void test_filter_custom_filters__initialize(void)
{
	cl_fixture_sandbox("empty_standard_repo");
	cl_git_pass(p_rename(
		"empty_standard_repo/.gitted", "empty_standard_repo/.git"));
	cl_git_pass(git_repository_open(&g_repo, "empty_standard_repo"));
}

void test_filter_custom_filters__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup("empty_standard_repo");
}

void test_filter_custom_filters__rawcontent_is_unfiltered(void)
{
	git_blob *blob;
	git_oid oid;

	/* _frombuffer() doesn't apply filters */
	cl_git_pass(git_blob_create_frombuffer(&oid, g_repo, "testme\r\n", 8));

	cl_git_pass(git_blob_lookup(&blob, g_repo, &oid));
	cl_assert(8 == git_blob_rawsize(blob));
	cl_assert_equal_s("testme\r\n", (char *)git_blob_rawcontent(blob));
	git_blob_free(blob);
}

void test_filter_custom_filters__stats(void)
{
	int i;
	git_blob *blob;
	git_oid oid;
	git_buf buf = GIT_BUF_INIT;
	git_buf_text_stats stats;
	char *raw[8] = {
		"",
		"foo\nbar\n",
		"foo\rbar\r",
		"foo\r\nbar\r\n",
		"foo\nbar\rboth\r\nreversed\n\ragain\nproblems\r",
		"123\n\000\001\002\003\004abc\255\254\253\r\n",
		"\xEF\xBB\xBFThis is UTF-8\n",
		"\xFE\xFF\x00T\x00h\x00i\x00s\x00!"
	};
	git_off_t len_raw[8] =
		{ 0, 8, 8, 10, 39, 17, 17, 12 };
	git_buf_text_stats expected_stats[8] = {
		{ 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 2, 0, 6, 0 },
		{ 0, 0, 2, 0, 0, 6, 0 },
		{ 0, 0, 2, 2, 2, 6, 0 },
		{ 0, 0, 4, 4, 1, 31, 0 },
		{ 0, 1, 1, 2, 1, 9, 5 },
		{ GIT_BOM_UTF8, 0, 0, 1, 0, 16, 0 },
		{ GIT_BOM_UTF16_BE, 5, 0, 0, 0, 7, 5 },
	};

	for (i = 0; i < 8; i++) {
		cl_git_pass(git_blob_create_frombuffer(&oid, g_repo, raw[i],
			len_raw[i]));

		cl_git_pass(git_blob_lookup(&blob, g_repo, &oid));
		cl_git_pass(git_blob__getbuf(&buf, blob));
		git_buf_text_gather_stats(&stats, &buf, false);
		cl_assert(memcmp(&expected_stats[i], &stats, sizeof(stats)) == 0);
		git_blob_free(blob);
	}

	git_buf_free(&buf);
}

void test_filter_custom_filters__crlf_filter_is_available_by_default(void)
{
	git_blob *blob;
	git_oid oid;

	git_attr_cache_flush(g_repo);
	cl_git_append2file("empty_standard_repo/.gitattributes", "*.txt text\n");

	cl_git_mkfile("empty_standard_repo/ping.txt", "pong\r\n");
	cl_git_pass(git_blob_create_fromworkdir(&oid, g_repo, "ping.txt"));

	cl_git_pass(git_blob_lookup(&blob, g_repo, &oid));
	cl_assert_equal_s("pong\n", (char *)git_blob_rawcontent(blob));
	
	git_blob_free(blob);
}

