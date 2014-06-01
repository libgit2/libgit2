#include "clar_libgit2.h"
#include "buffer.h"

static void expect_pass(const char *expected, const char *quoted)
{
	git_buf buf = GIT_BUF_INIT;

	cl_git_pass(git_buf_puts(&buf, quoted));
	cl_git_pass(git_buf_unquote(&buf));

	cl_assert_equal_s(expected, git_buf_cstr(&buf));
	cl_assert_equal_i(strlen(expected), git_buf_len(&buf));

	git_buf_free(&buf);
}

static void expect_fail(const char *quoted)
{
	git_buf buf = GIT_BUF_INIT;

	cl_git_pass(git_buf_puts(&buf, quoted));
	cl_git_fail(git_buf_unquote(&buf));

	git_buf_free(&buf);
}

void test_buf_quote__unquote_succeeds(void)
{
	expect_pass("", "\"\"");
	expect_pass(" ", "\" \"");
	expect_pass("foo", "\"foo\"");
	expect_pass("foo bar", "\"foo bar\"");
	expect_pass("foo\"bar", "\"foo\\\"bar\"");
	expect_pass("foo\\bar", "\"foo\\\\bar\"");
	expect_pass("foo\tbar", "\"foo\\tbar\"");
	expect_pass("\vfoo\tbar\n", "\"\\vfoo\\tbar\\n\"");
	expect_pass("foo\nbar", "\"foo\\012bar\"");
	expect_pass("foo\r\nbar", "\"foo\\015\\012bar\"");
	expect_pass("foo\r\nbar", "\"\\146\\157\\157\\015\\012\\142\\141\\162\"");
	expect_pass("newline: \n", "\"newline: \\012\"");
}

void test_buf_quote__unquote_fails(void)
{
	expect_fail("no quotes at all");
	expect_fail("\"no trailing quote");
	expect_fail("no leading quote\"");
	expect_fail("\"invalid \\z escape char\"");
	expect_fail("\"\\q invalid escape char\"");
	expect_fail("\"invalid escape char \\p\"");
	expect_fail("\"invalid \\1 escape char \"");
	expect_fail("\"invalid \\14 escape char \"");
	expect_fail("\"invalid \\411 escape char\"");
	expect_fail("\"truncated escape char \\\"");
	expect_fail("\"truncated escape char \\0\"");
	expect_fail("\"truncated escape char \\01\"");
}
