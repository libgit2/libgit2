#include "clar_libgit2.h"
#include "path.h"

static void test_make_relative(
	const char *expected_path,
	const char *path,
	const char *parent,
	int expected_status)
{
	git_buf buf = GIT_BUF_INIT;
	git_buf_puts(&buf, path);
	cl_assert_equal_i(expected_status, git_path_make_relative(&buf, parent));
	cl_assert_equal_s(expected_path, buf.ptr);
	git_buf_free(&buf);
}

void test_path_core__make_relative(void)
{
	git_buf buf = GIT_BUF_INIT;

	test_make_relative("foo.c", "/path/to/foo.c", "/path/to", 0);
	test_make_relative("bar/foo.c", "/path/to/bar/foo.c", "/path/to", 0);
	test_make_relative("foo.c", "/path/to/foo.c", "/path/to/", 0);

	test_make_relative("", "/path/to", "/path/to", 0);
	test_make_relative("", "/path/to", "/path/to/", 0);

	test_make_relative("../", "/path/to", "/path/to/foo", 0);

	test_make_relative("../foo.c", "/path/to/foo.c", "/path/to/bar", 0);
	test_make_relative("../bar/foo.c", "/path/to/bar/foo.c", "/path/to/baz", 0);

	test_make_relative("../../foo.c", "/path/to/foo.c", "/path/to/foo/bar", 0);
	test_make_relative("../../foo/bar.c", "/path/to/foo/bar.c", "/path/to/bar/foo", 0);

	test_make_relative("../../foo.c", "/foo.c", "/bar/foo", 0);

	test_make_relative("foo.c", "/path/to/foo.c", "/path/to/", 0);
	test_make_relative("../foo.c", "/path/to/foo.c", "/path/to/bar/", 0);

	test_make_relative("foo.c", "d:/path/to/foo.c", "d:/path/to", 0);

	test_make_relative("../foo", "/foo", "/bar", 0);
	test_make_relative("path/to/foo.c", "/path/to/foo.c", "/", 0);
	test_make_relative("../foo", "path/to/foo", "path/to/bar", 0);

	test_make_relative("/path/to/foo.c", "/path/to/foo.c", "d:/path/to", GIT_ENOTFOUND);
	test_make_relative("d:/path/to/foo.c", "d:/path/to/foo.c", "/path/to", GIT_ENOTFOUND);
	
	test_make_relative("/path/to/foo.c", "/path/to/foo.c", "not-a-rooted-path", GIT_ENOTFOUND);
	test_make_relative("not-a-rooted-path", "not-a-rooted-path", "/path/to", GIT_ENOTFOUND);
	
	test_make_relative("/path", "/path", "pathtofoo", GIT_ENOTFOUND);
	test_make_relative("path", "path", "pathtofoo", GIT_ENOTFOUND);
}
