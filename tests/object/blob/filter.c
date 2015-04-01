#include "clar_libgit2.h"
#include "posix.h"
#include "blob.h"
#include "buf_text.h"

static git_repository *g_repo = NULL;

#define CRLF_NUM_TEST_OBJECTS	9

static const char *g_crlf_raw[CRLF_NUM_TEST_OBJECTS] = {
	"",
	"foo\nbar\n",
	"foo\rbar\r",
	"foo\r\nbar\r\n",
	"foo\nbar\rboth\r\nreversed\n\ragain\nproblems\r",
	"123\n\000\001\002\003\004abc\255\254\253\r\n",
	"\xEF\xBB\xBFThis is UTF-8\n",
	"\xEF\xBB\xBF\xE3\x81\xBB\xE3\x81\x92\xE3\x81\xBB\xE3\x81\x92\r\n\xE3\x81\xBB\xE3\x81\x92\xE3\x81\xBB\xE3\x81\x92\r\n",
	"\xFE\xFF\x00T\x00h\x00i\x00s\x00!"
};

static git_off_t g_crlf_raw_len[CRLF_NUM_TEST_OBJECTS] = {
	-1, -1, -1, -1, -1, 17, -1, -1, 12
};

static git_oid g_crlf_oids[CRLF_NUM_TEST_OBJECTS];

static git_buf g_crlf_filtered[CRLF_NUM_TEST_OBJECTS] = {
	{ "", 0, 0 },
	{ "foo\nbar\n", 0, 8 },
	{ "foo\rbar\r", 0, 8 },
	{ "foo\nbar\n", 0, 8 },
	{ "foo\nbar\rboth\nreversed\n\ragain\nproblems\r", 0, 38 },
	{ "123\n\000\001\002\003\004abc\255\254\253\n", 0, 16 },
	{ "\xEF\xBB\xBFThis is UTF-8\n", 0, 17 },
	{ "\xEF\xBB\xBF\xE3\x81\xBB\xE3\x81\x92\xE3\x81\xBB\xE3\x81\x92\n\xE3\x81\xBB\xE3\x81\x92\xE3\x81\xBB\xE3\x81\x92\n", 0, 29 },
	{ "\xFE\xFF\x00T\x00h\x00i\x00s\x00!", 0, 12 }
};

static git_buf_text_stats g_crlf_filtered_stats[CRLF_NUM_TEST_OBJECTS] = {
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 2, 0, 6, 0 },
	{ 0, 0, 2, 0, 0, 6, 0 },
	{ 0, 0, 2, 2, 2, 6, 0 },
	{ 0, 0, 4, 4, 1, 31, 0 },
	{ 0, 1, 1, 2, 1, 9, 5 },
	{ GIT_BOM_UTF8, 0, 0, 1, 0, 16, 0 },
	{ GIT_BOM_UTF8, 0, 2, 2, 2, 27, 0 },
	{ GIT_BOM_UTF16_BE, 5, 0, 0, 0, 7, 5 },
};

void test_object_blob_filter__initialize(void)
{
	int i;

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	for (i = 0; i < CRLF_NUM_TEST_OBJECTS; i++) {
		if (g_crlf_raw_len[i] < 0)
			g_crlf_raw_len[i] = strlen(g_crlf_raw[i]);

		cl_git_pass(git_blob_create_frombuffer(
			&g_crlf_oids[i], g_repo, g_crlf_raw[i], (size_t)g_crlf_raw_len[i]));
	}
}

void test_object_blob_filter__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_object_blob_filter__unfiltered(void)
{
	int i;
	git_blob *blob;

	for (i = 0; i < CRLF_NUM_TEST_OBJECTS; i++) {
		size_t raw_len = (size_t)g_crlf_raw_len[i];

		cl_git_pass(git_blob_lookup(&blob, g_repo, &g_crlf_oids[i]));

		cl_assert_equal_sz(raw_len, (size_t)git_blob_rawsize(blob));
		cl_assert_equal_i(
			0, memcmp(g_crlf_raw[i], git_blob_rawcontent(blob), raw_len));

		git_blob_free(blob);
	}
}

void test_object_blob_filter__stats(void)
{
	int i;
	git_blob *blob;
	git_buf buf = GIT_BUF_INIT;
	git_buf_text_stats stats;

	for (i = 0; i < CRLF_NUM_TEST_OBJECTS; i++) {
		cl_git_pass(git_blob_lookup(&blob, g_repo, &g_crlf_oids[i]));
		cl_git_pass(git_blob__getbuf(&buf, blob));
		git_buf_text_gather_stats(&stats, &buf, false);
		cl_assert_equal_i(
			0, memcmp(&g_crlf_filtered_stats[i], &stats, sizeof(stats)));
		git_blob_free(blob);
	}

	git_buf_free(&buf);
}

