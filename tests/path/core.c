#include "clar_libgit2.h"
#include "path.h"

void test_path_core__isvalid_standard(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar/file.txt", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar/.file", 0));
}

void test_path_core__isvalid_empty_dir_component(void)
{
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo//bar", 0));

	/* leading slash */
	cl_assert_equal_b(false, git_path_isvalid(NULL, "/", 0));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "/foo", 0));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "/foo/bar", 0));

	/* trailing slash */
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/", 0));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/bar/", 0));
}

void test_path_core__isvalid_dot_and_dotdot(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, ".", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "./foo", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/.", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "./foo", 0));

	cl_assert_equal_b(true, git_path_isvalid(NULL, "..", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "../foo", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/..", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "../foo", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, ".", GIT_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "./foo", GIT_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/.", GIT_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "./foo", GIT_PATH_REJECT_TRAVERSAL));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "..", GIT_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "../foo", GIT_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/..", GIT_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "../foo", GIT_PATH_REJECT_TRAVERSAL));
}

void test_path_core__isvalid_dot_git(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, ".git", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, ".git/foo", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/.git", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/.git/bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/.GIT/bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar/.Git", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, ".git", GIT_PATH_REJECT_DOT_GIT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, ".git/foo", GIT_PATH_REJECT_DOT_GIT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/.git", GIT_PATH_REJECT_DOT_GIT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/.git/bar", GIT_PATH_REJECT_DOT_GIT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/.GIT/bar", GIT_PATH_REJECT_DOT_GIT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/bar/.Git", GIT_PATH_REJECT_DOT_GIT));

	cl_assert_equal_b(true, git_path_isvalid(NULL, "!git", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/!git", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "!git/bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, ".tig", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/.tig", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, ".tig/bar", 0));
}

void test_path_core__isvalid_backslash(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo\\file.txt", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar\\file.txt", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar\\", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo\\file.txt", GIT_PATH_REJECT_BACKSLASH));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/bar\\file.txt", GIT_PATH_REJECT_BACKSLASH));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/bar\\", GIT_PATH_REJECT_BACKSLASH));
}

void test_path_core__isvalid_trailing_dot(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo.", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo...", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar.", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo./bar", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo.", GIT_PATH_REJECT_TRAILING_DOT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo...", GIT_PATH_REJECT_TRAILING_DOT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/bar.", GIT_PATH_REJECT_TRAILING_DOT));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo./bar", GIT_PATH_REJECT_TRAILING_DOT));
}

void test_path_core__isvalid_trailing_space(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo ", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo   ", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar ", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, " ", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo /bar", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo ", GIT_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo   ", GIT_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/bar ", GIT_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_path_isvalid(NULL, " ", GIT_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo /bar", GIT_PATH_REJECT_TRAILING_SPACE));
}

void test_path_core__isvalid_trailing_colon(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo:", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo/bar:", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, ":", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "foo:/bar", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo:", GIT_PATH_REJECT_TRAILING_COLON));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo/bar:", GIT_PATH_REJECT_TRAILING_COLON));
	cl_assert_equal_b(false, git_path_isvalid(NULL, ":", GIT_PATH_REJECT_TRAILING_COLON));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "foo:/bar", GIT_PATH_REJECT_TRAILING_COLON));
}

void test_path_core__isvalid_dos_git_shortname(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "git~1", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "git~1", GIT_PATH_REJECT_DOS_GIT_SHORTNAME));
}

void test_path_core__isvalid_dos_paths(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux.", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux:", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux.asdf", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux.asdf\\zippy", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux:asdf\\foobar", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "aux", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "aux.", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "aux:", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "aux.asdf", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "aux.asdf\\zippy", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "aux:asdf\\foobar", GIT_PATH_REJECT_DOS_PATHS));

	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux1", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux1", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "auxn", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "aux\\foo", GIT_PATH_REJECT_DOS_PATHS));
}

void test_path_core__isvalid_dos_paths_withnum(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com1", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com1.", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com1:", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com1.asdf", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com1.asdf\\zippy", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com1:asdf\\foobar", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "com1", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "com1.", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "com1:", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "com1.asdf", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "com1.asdf\\zippy", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "com1:asdf\\foobar", GIT_PATH_REJECT_DOS_PATHS));

	cl_assert_equal_b(true, git_path_isvalid(NULL, "com10", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com10", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "comn", GIT_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "com1\\foo", GIT_PATH_REJECT_DOS_PATHS));
}

void test_core_path__isvalid_nt_chars(void)
{
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf\001foo", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf\037bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf<bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf>foo", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf:foo", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf\"bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf|foo", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf?bar", 0));
	cl_assert_equal_b(true, git_path_isvalid(NULL, "asdf*bar", 0));

	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf\001foo", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf\037bar", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf<bar", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf>foo", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf:foo", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf\"bar", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf|foo", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf?bar", GIT_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_path_isvalid(NULL, "asdf*bar", GIT_PATH_REJECT_NT_CHARS));
}
