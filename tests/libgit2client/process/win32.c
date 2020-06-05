#include "git2client_tests.h"
#include "process.h"
#include "vector.h"

#ifdef GIT_WIN32
static git_strarray input;
static git_buf result;

# define assert_cmdline(expected, given) do { \
		cl_git_pass(git_process__cmdline(&result, given, ARRAY_SIZE(given))); \
		cl_assert_equal_s(expected, result.ptr); \
		git_buf_dispose(&result); \
	} while(0)
#endif

void test_process_win32__cmdline_is_whitespace_delimited(void)
{
#ifdef GIT_WIN32
	char *one[] = { "one" };
	char *two[] = { "one", "two" };
	char *three[] = { "one", "two", "three" };
	char *four[] = { "one", "two", "three", "four" };

	assert_cmdline("one", one);
	assert_cmdline("one two", two);
	assert_cmdline("one two three", three);
	assert_cmdline("one two three four", four);
#endif
}

void test_process_win32__cmdline_escapes_whitespace(void)
{
#ifdef GIT_WIN32
	char *spaces[] = { "one with spaces" };
	char *tabs[] = { "one\twith\ttabs" };
	char *multiple[] = { "one    with    many    spaces" };

	assert_cmdline("one\" \"with\" \"spaces", spaces);
	assert_cmdline("one\"\t\"with\"\t\"tabs", tabs);
	assert_cmdline("one\"    \"with\"    \"many\"    \"spaces", multiple);
#endif
}

void test_process_win32__cmdline_escapes_quotes(void)
{
#ifdef GIT_WIN32
	char *one[] = { "echo", "\"hello world\"" };

	assert_cmdline("echo \\\"hello\" \"world\\\"", one);
#endif
}

void test_process_win32__cmdline_escapes_backslash(void)
{
#ifdef GIT_WIN32
	char *one[] = { "foo\\bar", "foo\\baz" };
	char *two[] = { "c:\\program files\\foo bar\\foo bar.exe", "c:\\path\\to\\other\\", "/a", "/b" };

	assert_cmdline("foo\\\\bar foo\\\\baz", one);
	assert_cmdline("c:\\\\program\" \"files\\\\foo\" \"bar\\\\foo\" \"bar.exe c:\\\\path\\\\to\\\\other\\\\ /a /b", two);
#endif
}
