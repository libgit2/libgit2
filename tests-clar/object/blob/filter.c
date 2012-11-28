#include "clar_libgit2.h"
#include "posix.h"
#include "blob.h"
#include "filter.h"

static git_repository *g_repo = NULL;
#define NUM_TEST_OBJECTS 8
static git_oid g_oids[NUM_TEST_OBJECTS];
static const char *g_raw[NUM_TEST_OBJECTS] = {
	"",
	"foo\nbar\n",
	"foo\rbar\r",
	"foo\r\nbar\r\n",
	"foo\nbar\rboth\r\nreversed\n\ragain\nproblems\r",
	"123\n\000\001\002\003\004abc\255\254\253\r\n",
	"\xEF\xBB\xBFThis is UTF-8\n",
	"\xFE\xFF\x00T\x00h\x00i\x00s\x00!"
};
static git_off_t g_len[NUM_TEST_OBJECTS] = { -1, -1, -1, -1, -1, 17, -1, 12 };
static git_buf_text_stats g_stats[NUM_TEST_OBJECTS] = {
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 2, 0, 6, 0 },
	{ 0, 0, 2, 0, 0, 6, 0 },
	{ 0, 0, 2, 2, 2, 6, 0 },
	{ 0, 0, 4, 4, 1, 31, 0 },
	{ 0, 1, 1, 2, 1, 9, 5 },
	{ GIT_BOM_UTF8, 0, 0, 1, 0, 16, 0 },
	{ GIT_BOM_UTF16_BE, 5, 0, 0, 0, 7, 5 },
};
static git_buf g_crlf_filtered[NUM_TEST_OBJECTS] = {
	{ "", 0, 0 },
	{ "foo\nbar\n", 0, 8 },
	{ "foo\rbar\r", 0, 8 },
	{ "foo\nbar\n", 0, 8 },
	{ "foo\nbar\rboth\nreversed\n\ragain\nproblems\r", 0, 38 },
	{ "123\n\000\001\002\003\004abc\255\254\253\n", 0, 16 },
	{ "\xEF\xBB\xBFThis is UTF-8\n", 0, 17 },
	{ "\xFE\xFF\x00T\x00h\x00i\x00s\x00!", 0, 12 }
};

void test_object_blob_filter__initialize(void)
{
	int i;

	cl_fixture_sandbox("empty_standard_repo");
	cl_git_pass(p_rename(
		"empty_standard_repo/.gitted", "empty_standard_repo/.git"));
	cl_git_pass(git_repository_open(&g_repo, "empty_standard_repo"));

	for (i = 0; i < NUM_TEST_OBJECTS; i++) {
		size_t len = (g_len[i] < 0) ? strlen(g_raw[i]) : (size_t)g_len[i];
		g_len[i] = (git_off_t)len;

		cl_git_pass(
			git_blob_create_frombuffer(&g_oids[i], g_repo, g_raw[i], len)
		);
	}
}

void test_object_blob_filter__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup("empty_standard_repo");
}

void test_object_blob_filter__unfiltered(void)
{
	int i;
	git_blob *blob;

	for (i = 0; i < NUM_TEST_OBJECTS; i++) {
		cl_git_pass(git_blob_lookup(&blob, g_repo, &g_oids[i]));
		cl_assert(g_len[i] == git_blob_rawsize(blob));
		cl_assert(memcmp(git_blob_rawcontent(blob), g_raw[i], (size_t)g_len[i]) == 0);
		git_blob_free(blob);
	}
}

void test_object_blob_filter__stats(void)
{
	int i;
	git_blob *blob;
	git_buf buf = GIT_BUF_INIT;
	git_buf_text_stats stats;

	for (i = 0; i < NUM_TEST_OBJECTS; i++) {
		cl_git_pass(git_blob_lookup(&blob, g_repo, &g_oids[i]));
		cl_git_pass(git_blob__getbuf(&buf, blob));
		git_buf_text_gather_stats(&stats, &buf, false);
		cl_assert(memcmp(&g_stats[i], &stats, sizeof(stats)) == 0);
		git_blob_free(blob);
	}

	git_buf_free(&buf);
}

void test_object_blob_filter__to_odb(void)
{
	git_vector filters = GIT_VECTOR_INIT;
	git_config *cfg;
	int i;
	git_blob *blob;
	git_buf orig = GIT_BUF_INIT, out = GIT_BUF_INIT;

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_assert(cfg);

	git_attr_cache_flush(g_repo);
	cl_git_append2file("empty_standard_repo/.gitattributes", "*.txt text\n");

	cl_assert(git_filters_load(
		&filters, g_repo, "filename.txt", GIT_FILTER_TO_ODB) > 0);
	cl_assert(filters.length == 1);

	for (i = 0; i < NUM_TEST_OBJECTS; i++) {
		cl_git_pass(git_blob_lookup(&blob, g_repo, &g_oids[i]));
		cl_git_pass(git_blob__getbuf(&orig, blob));

		cl_git_pass(git_filters_apply(&out, &orig, &filters));
		cl_assert(git_buf_cmp(&out, &g_crlf_filtered[i]) == 0);

		git_blob_free(blob);
	}

	git_filters_free(&filters);
	git_buf_free(&orig);
	git_buf_free(&out);
	git_config_free(cfg);
}

