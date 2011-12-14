#include "clay_libgit2.h"
#include <fileops.h>

static void
check_dirname(const char *A, const char *B)
{
	git_buf dir = GIT_BUF_INIT;
	char *dir2;

	cl_assert(git_path_dirname_r(&dir, A) >= 0);
	cl_assert_strequal(B, dir.ptr);
	git_buf_free(&dir);

	cl_assert((dir2 = git_path_dirname(A)) != NULL);
	cl_assert_strequal(B, dir2);
	git__free(dir2);
}

static void
check_basename(const char *A, const char *B)
{
	git_buf base = GIT_BUF_INIT;
	char *base2;

	cl_assert(git_path_basename_r(&base, A) >= 0);
	cl_assert_strequal(B, base.ptr);
	git_buf_free(&base);

	cl_assert((base2 = git_path_basename(A)) != NULL);
	cl_assert_strequal(B, base2);
	git__free(base2);
}

static void
check_topdir(const char *A, const char *B)
{
	const char *dir;

	cl_assert((dir = git_path_topdir(A)) != NULL);
	cl_assert_strequal(B, dir);
}

static void
check_joinpath(const char *path_a, const char *path_b, const char *expected_path)
{
	git_buf joined_path = GIT_BUF_INIT;

	cl_git_pass(git_buf_joinpath(&joined_path, path_a, path_b));
	cl_assert_strequal(expected_path, joined_path.ptr);

	git_buf_free(&joined_path);
}

static void
check_joinpath_n(
	const char *path_a,
	const char *path_b,
	const char *path_c,
	const char *path_d,
	const char *expected_path)
{
	git_buf joined_path = GIT_BUF_INIT;

	cl_git_pass(git_buf_join_n(&joined_path, '/', 4,
							   path_a, path_b, path_c, path_d));
	cl_assert_strequal(expected_path, joined_path.ptr);

	git_buf_free(&joined_path);
}


/* get the dirname of a path */
void test_core_path__0_dirname(void)
{
	check_dirname(NULL, ".");
	check_dirname("", ".");
	check_dirname("a", ".");
	check_dirname("/", "/");
	check_dirname("/usr", "/");
	check_dirname("/usr/", "/");
	check_dirname("/usr/lib", "/usr");
	check_dirname("/usr/lib/", "/usr");
	check_dirname("/usr/lib//", "/usr");
	check_dirname("usr/lib", "usr");
	check_dirname("usr/lib/", "usr");
	check_dirname("usr/lib//", "usr");
	check_dirname(".git/", ".");

	check_dirname(REP16("/abc"), REP15("/abc"));
}

/* get the base name of a path */
void test_core_path__1_basename(void)
{
	check_basename(NULL, ".");
	check_basename("", ".");
	check_basename("a", "a");
	check_basename("/", "/");
	check_basename("/usr", "usr");
	check_basename("/usr/", "usr");
	check_basename("/usr/lib", "lib");
	check_basename("/usr/lib//", "lib");
	check_basename("usr/lib", "lib");

	check_basename(REP16("/abc"), "abc");
	check_basename(REP1024("/abc"), "abc");
}

/* get the latest component in a path */
void test_core_path__2_topdir(void)
{
	check_topdir(".git/", ".git/");
	check_topdir("/.git/", ".git/");
	check_topdir("usr/local/.git/", ".git/");
	check_topdir("./.git/", ".git/");
	check_topdir("/usr/.git/", ".git/");
	check_topdir("/", "/");
	check_topdir("a/", "a/");

	cl_assert(git_path_topdir("/usr/.git") == NULL);
	cl_assert(git_path_topdir(".") == NULL);
	cl_assert(git_path_topdir("") == NULL);
	cl_assert(git_path_topdir("a") == NULL);
}

/* properly join path components */
void test_core_path__5_joins(void)
{
	check_joinpath("", "", "");
	check_joinpath("", "a", "a");
	check_joinpath("", "/a", "/a");
	check_joinpath("a", "", "a/");
	check_joinpath("a", "/", "a/");
	check_joinpath("a", "b", "a/b");
	check_joinpath("/", "a", "/a");
	check_joinpath("/", "", "/");
	check_joinpath("/a", "/b", "/a/b");
	check_joinpath("/a", "/b/", "/a/b/");
	check_joinpath("/a/", "b/", "/a/b/");
	check_joinpath("/a/", "/b/", "/a/b/");

	check_joinpath("/abcd", "/defg", "/abcd/defg");
	check_joinpath("/abcd", "/defg/", "/abcd/defg/");
	check_joinpath("/abcd/", "defg/", "/abcd/defg/");
	check_joinpath("/abcd/", "/defg/", "/abcd/defg/");

	check_joinpath("/abcdefgh", "/12345678", "/abcdefgh/12345678");
	check_joinpath("/abcdefgh", "/12345678/", "/abcdefgh/12345678/");
	check_joinpath("/abcdefgh/", "12345678/", "/abcdefgh/12345678/");

	check_joinpath(REP1024("aaaa"), "", REP1024("aaaa") "/");
	check_joinpath(REP1024("aaaa/"), "", REP1024("aaaa/"));
	check_joinpath(REP1024("/aaaa"), "", REP1024("/aaaa") "/");

	check_joinpath(REP1024("aaaa"), REP1024("bbbb"),
				   REP1024("aaaa") "/" REP1024("bbbb"));
	check_joinpath(REP1024("/aaaa"), REP1024("/bbbb"),
				   REP1024("/aaaa") REP1024("/bbbb"));
}

/* properly join path components for more than one path */
void test_core_path__6_long_joins(void)
{
	check_joinpath_n("", "", "", "", "");
	check_joinpath_n("", "a", "", "", "a/");
	check_joinpath_n("a", "", "", "", "a/");
	check_joinpath_n("", "", "", "a", "a");
	check_joinpath_n("a", "b", "", "/c/d/", "a/b/c/d/");
	check_joinpath_n("a", "b", "", "/c/d", "a/b/c/d");
	check_joinpath_n("abcd", "efgh", "ijkl", "mnop", "abcd/efgh/ijkl/mnop");
	check_joinpath_n("abcd/", "efgh/", "ijkl/", "mnop/", "abcd/efgh/ijkl/mnop/");
	check_joinpath_n("/abcd/", "/efgh/", "/ijkl/", "/mnop/", "/abcd/efgh/ijkl/mnop/");

	check_joinpath_n(REP1024("a"), REP1024("b"), REP1024("c"), REP1024("d"),
					 REP1024("a") "/" REP1024("b") "/"
					 REP1024("c") "/" REP1024("d"));
	check_joinpath_n(REP1024("/a"), REP1024("/b"), REP1024("/c"), REP1024("/d"),
					 REP1024("/a") REP1024("/b")
					 REP1024("/c") REP1024("/d"));
}


static void
check_path_to_dir(
	const char* path,
    const char* expected)
{
	git_buf tgt = GIT_BUF_INIT;

	git_buf_sets(&tgt, path);
	cl_git_pass(git_path_to_dir(&tgt));
	cl_assert_strequal(expected, tgt.ptr);

	git_buf_free(&tgt);
}

static void
check_string_to_dir(
	const char* path,
	int         maxlen,
    const char* expected)
{
	int  len = strlen(path);
	char *buf = git__malloc(len + 2);
	strncpy(buf, path, len + 2);

	git_path_string_to_dir(buf, maxlen);

	cl_assert_strequal(expected, buf);

	git__free(buf);
}

/* convert paths to dirs */
void test_core_path__7_path_to_dir(void)
{
	check_path_to_dir("", "");
	check_path_to_dir(".", "./");
	check_path_to_dir("./", "./");
	check_path_to_dir("a/", "a/");
	check_path_to_dir("ab", "ab/");
	/* make sure we try just under and just over an expansion that will
	 * require a realloc
	 */
	check_path_to_dir("abcdef", "abcdef/");
	check_path_to_dir("abcdefg", "abcdefg/");
	check_path_to_dir("abcdefgh", "abcdefgh/");
	check_path_to_dir("abcdefghi", "abcdefghi/");
	check_path_to_dir(REP1024("abcd") "/", REP1024("abcd") "/");
	check_path_to_dir(REP1024("abcd"), REP1024("abcd") "/");

	check_string_to_dir("", 1, "");
	check_string_to_dir(".", 1, ".");
	check_string_to_dir(".", 2, "./");
	check_string_to_dir(".", 3, "./");
	check_string_to_dir("abcd", 3, "abcd");
	check_string_to_dir("abcd", 4, "abcd");
	check_string_to_dir("abcd", 5, "abcd/");
	check_string_to_dir("abcd", 6, "abcd/");
}

/* join path to itself */
void test_core_path__8_self_join(void)
{
	git_buf path = GIT_BUF_INIT;
	ssize_t asize = 0;

	asize = path.asize;
	cl_git_pass(git_buf_sets(&path, "/foo"));
	cl_assert_strequal(path.ptr, "/foo");
	cl_assert(asize < path.asize);

	asize = path.asize;
	cl_git_pass(git_buf_joinpath(&path, path.ptr, "this is a new string"));
	cl_assert_strequal(path.ptr, "/foo/this is a new string");
	cl_assert(asize < path.asize);

	asize = path.asize;
	cl_git_pass(git_buf_joinpath(&path, path.ptr, "/grow the buffer, grow the buffer, grow the buffer"));
	cl_assert_strequal(path.ptr, "/foo/this is a new string/grow the buffer, grow the buffer, grow the buffer");
	cl_assert(asize < path.asize);

	git_buf_free(&path);
	cl_git_pass(git_buf_sets(&path, "/foo/bar"));

	cl_git_pass(git_buf_joinpath(&path, path.ptr + 4, "baz"));
	cl_assert_strequal(path.ptr, "/bar/baz");

	asize = path.asize;
	cl_git_pass(git_buf_joinpath(&path, path.ptr + 4, "somethinglongenoughtorealloc"));
	cl_assert_strequal(path.ptr, "/baz/somethinglongenoughtorealloc");
	cl_assert(asize < path.asize);
	
	git_buf_free(&path);
}
