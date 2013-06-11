#ifndef INCLUDE_cl_crlf_data_h__
#define INCLUDE_cl_crlf_data_h__

#define NUM_CRLF_TEST_OBJECTS 9

static const char *g_raw[NUM_CRLF_TEST_OBJECTS] = {
	"",
	"foo\nbar\n",
	"foo\rbar\r",
	"foo\r\nbar\r\n",
	"foo\nbar\rboth\r\nreversed\n\ragain\nproblems\r",
	"123\n\000\001\002\003\004abc\255\254\253\r\n",
	"123\n\000\001\002\003\004abc\255\254\253\n",
	"\xEF\xBB\xBFThis is UTF-8\n",
	"\xFE\xFF\x00T\x00h\x00i\x00s\x00!"
};
static git_off_t g_len_raw[NUM_CRLF_TEST_OBJECTS] =
	{ 0, 8, 8, 10, 39, 17, 16, 17, 12 };

static const char *g_crlf_filtered_to_odb[NUM_CRLF_TEST_OBJECTS] = {
	"",
	"foo\nbar\n",
	"foo\rbar\r",
	"foo\nbar\n",
	"foo\nbar\rboth\nreversed\n\ragain\nproblems\r",
	"123\n\000\001\002\003\004abc\255\254\253\n",
	"123\n\000\001\002\003\004abc\255\254\253\n",
	"\xEF\xBB\xBFThis is UTF-8\n",
	"\xFE\xFF\x00T\x00h\x00i\x00s\x00!"
};
static git_off_t g_len_crlf_filtered_to_odb[NUM_CRLF_TEST_OBJECTS] =
	{ 0, 8, 8, 8, 38, 16, 16, 17, 12 };

static const char *g_crlf_filtered_to_worktree[NUM_CRLF_TEST_OBJECTS] = {
	"",
	"foo\r\nbar\r\n",
	"foo\rbar\r",
	"foo\r\nbar\r\n",
	"foo\r\nbar\rboth\r\nreversed\r\n\ragain\r\nproblems\r",
	"123\r\n\000\001\002\003\004abc\255\254\253\r\n",
	"123\r\n\000\001\002\003\004abc\255\254\253\r\n",
	"\xEF\xBB\xBFThis is UTF-8\r\n",
	"\xFE\xFF\x00T\x00h\x00i\x00s\x00!"
};
static git_off_t g_len_crlf_filtered_to_worktree[NUM_CRLF_TEST_OBJECTS] =
	{ 0, 10, 8, 10, 42, 18, 18, 18, 12 };

static int g_to_odb_expected_ret[NUM_CRLF_TEST_OBJECTS] = { -1, -3, 0, 0, 0, 0, -3, -3, -3 };
static int g_to_worktree_expected_ret[NUM_CRLF_TEST_OBJECTS] = { -1, 0, -3, 0, 0, 0, 0, 0, -3 };

#endif
