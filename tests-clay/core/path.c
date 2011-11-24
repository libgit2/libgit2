#include "clay_libgit2.h"
#include <fileops.h>

static void
check_dirname(const char *A, const char *B)
{
	git_path dir = GIT_PATH_INIT;
	char *dir2;

	cl_assert(git_path_dirname_r(&dir, A) >= 0);
	cl_assert_strequal(B, dir.data);
	git_path_free(&dir);

	cl_assert((dir2 = git_path_dirname(A)) != NULL);
	cl_assert_strequal(B, dir2);
	git__free(dir2);
}

static void
check_basename(const char *A, const char *B)
{
	git_path base = GIT_PATH_INIT;
	char *base2;

	cl_assert(git_path_basename_r(&base, A) >= 0);
	cl_assert_strequal(B, base.data);
	git_path_free(&base);

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
	git_path joined_path = GIT_PATH_INIT;
	cl_git_pass(git_path_join(&joined_path, path_a, path_b));
	cl_assert_strequal(expected_path, joined_path.data);
	git_path_free(&joined_path);
}

static void
check_joinpath_n(
	const char *path_a,
	const char *path_b,
	const char *path_c,
	const char *path_d,
	const char *expected_path)
{
	git_path joined_path = GIT_PATH_INIT;
	cl_git_pass(git_path_join_n(&joined_path, 4, path_a, path_b, path_c, path_d));
	cl_assert_strequal(expected_path, joined_path.data);
	git_path_free(&joined_path);
}

static void
check_path_append(
	const char* path_a,
	const char* path_b,
	const char* expected_path,
	size_t expected_size)
{
	git_path tgt = GIT_PATH_INIT_STR(path_a);

	cl_git_pass(git_path_strcat(&tgt, path_b));
	cl_assert_strequal(expected_path, tgt.data);
	cl_assert(tgt.size == expected_size);

	git_path_free(&tgt);
}

static void
check_path_append_2(
	const char* path_a,
	const char* path_b,
	const char* path_c,
	const char* expected_ab,
	const char* expected_abc,
	const char* expected_abca,
	const char* expected_abcab,
	const char* expected_abcabc)
{
	git_path tgt = GIT_PATH_INIT_STR(path_a);

	cl_git_pass(git_path_strcat(&tgt, path_b));
	cl_assert_strequal(expected_ab, tgt.data);
	cl_git_pass(git_path_strcat(&tgt, path_c));
	cl_assert_strequal(expected_abc, tgt.data);
	cl_git_pass(git_path_strcat(&tgt, path_a));
	cl_assert_strequal(expected_abca, tgt.data);
	cl_git_pass(git_path_strcat(&tgt, path_b));
	cl_assert_strequal(expected_abcab, tgt.data);
	cl_git_pass(git_path_strcat(&tgt, path_c));
	cl_assert_strequal(expected_abcabc, tgt.data);

	git_path_free(&tgt);
}

static void
check_path_strncat(
	const char* path_a,
	const char* path_b,
	size_t		len,
	const char* expected_path,
	size_t expected_size)
{
	git_path tgt = GIT_PATH_INIT_STR(path_a);

	cl_git_pass(git_path_strncat(&tgt, path_b, len));
	cl_assert_strequal(expected_path, tgt.data);
	cl_assert(tgt.size == expected_size);

	git_path_free(&tgt);
}

static void
check_path_as_dir(
	const char* path,
    const char* expected)
{
	git_path tgt = GIT_PATH_INIT_STR(path);

	cl_git_pass(git_path_as_dir(&tgt));
	cl_assert_strequal(expected, tgt.data);

	git_path_free(&tgt);
}

static void
check_string_as_dir(
	const char* path,
	int         maxlen,
    const char* expected)
{
	int  len = strlen(path);
	char *buf = git__malloc(len + 2);
	strncpy(buf, path, len + 2);

	git_path_string_as_dir(buf, maxlen);

	cl_assert_strequal(expected, buf);

	git__free(buf);
}

/* get the dirname of a path */
void test_core_path__0(void)
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
}

/* get the base name of a path */
void test_core_path__1(void)
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
}

/* get the latest component in a path */
void test_core_path__2(void)
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
void test_core_path__5(void)
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
}

/* properly join path components for more than one path */
void test_core_path__6(void)
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
}

#define REP4(STR)	 STR STR STR STR
#define REP16(STR)	 REP4(REP4(STR))
#define REP1024(STR) REP16(REP16(REP4(STR)))
#define TESTSTR_4096 REP1024("....")
#define TESTSTR_8192 REP1024("........")

/* test basic path object manipulation */
void test_core_path__7(void)
{
	const char *str_4096 = TESTSTR_4096;
	const char *str_8192 = TESTSTR_8192;

	/* prevent basic programmer error first */
	cl_assert(strlen(str_4096) == 4096);
	cl_assert(strlen(str_8192) == 8192);

	check_path_append(NULL, NULL, NULL, 0);
	check_path_append(NULL, "", "", 8); /* causes an alloc which is min value 8 */
	check_path_append("", NULL, "", 1); /* first arg is allocated at exact length */
	check_path_append("", "", "", 1);
	check_path_append("a", NULL, "a", 2);
	check_path_append(NULL, "a", "a", 8);
	check_path_append("", "a", "a", 8);
	check_path_append("a", "", "a", 2); /* nothing to add, so initial string is not reallocated */
	check_path_append("a", "b", "ab", 8);
	check_path_append("", "abcdefgh", "abcdefgh", 16);
	check_path_append("abcdefgh", "", "abcdefgh", 9);
	check_path_append("abcdefgh", "/", "abcdefgh/", 16);
	check_path_append("abcdefgh", "ijklmnop", "abcdefghijklmnop", 24);
	check_path_append(str_4096, NULL, str_4096, 4097);
	check_path_append(str_4096, "", str_4096, 4097);
	check_path_append(str_4096, str_4096, str_8192, 8200);

	check_path_append_2("a", "b", "c", "ab", "abc", "abca", "abcab", "abcabc");
	check_path_append_2("a1", "b2", "c3", "a1b2", "a1b2c3", "a1b2c3a1", "a1b2c3a1b2", "a1b2c3a1b2c3");
	check_path_append_2("a1/", "b2/", "c3/", "a1/b2/", "a1/b2/c3/", "a1/b2/c3/a1/", "a1/b2/c3/a1/b2/", "a1/b2/c3/a1/b2/c3/");

	check_path_strncat(NULL, NULL, 0, NULL, 0);
	check_path_strncat(NULL, NULL, 1, NULL, 0);
	check_path_strncat(NULL, "a", 0, "", 8);
	check_path_strncat(NULL, "a", 1, "a", 8);
	check_path_strncat(NULL, "a", 2, "a", 8);
	check_path_strncat(NULL, "a", 3, "a", 8);
	check_path_strncat(NULL, "ab", 0, "", 8);
	check_path_strncat(NULL, "ab", 1, "a", 8);
	check_path_strncat(NULL, "ab", 2, "ab", 8);
	check_path_strncat(NULL, "ab", 3, "ab", 8);
	check_path_strncat("ab", "cd", 0, "ab", 3);
	check_path_strncat("ab", "cd", 1, "abc", 8);
	check_path_strncat("ab", "cd", 2, "abcd", 8);
	check_path_strncat("ab", "cd", 3, "abcd", 8);
	check_path_strncat("ab", "cd", 4, "abcd", 8);
	check_path_strncat("abcd", "efgh", 0, "abcd", 5);
	check_path_strncat("abcd", "efgh", 1, "abcde", 8);
	check_path_strncat("abcd", "efgh", 2, "abcdef", 8);
	check_path_strncat("abcd", "efgh", 3, "abcdefg", 8);
	check_path_strncat("abcd", "efgh", 4, "abcdefgh", 16);
}


/* properly join path components for more than one path */
void test_core_path__8(void)
{
	check_path_as_dir(NULL, NULL);
	check_path_as_dir("", "");
	check_path_as_dir(".", "./");
	check_path_as_dir("./", "./");
	check_path_as_dir("a/", "a/");
	check_path_as_dir("ab", "ab/");
	/* make sure we try just under and just over an expansion that will
	 * require a realloc
	 */
	check_path_as_dir("abcdef", "abcdef/");
	check_path_as_dir("abcdefg", "abcdefg/");
	check_path_as_dir("abcdefgh", "abcdefgh/");
	check_path_as_dir(REP1024("abcd") "/", REP1024("abcd") "/");
	check_path_as_dir(REP1024("abcd"), REP1024("abcd") "/");

	check_string_as_dir("", 1, "");
	check_string_as_dir(".", 1, ".");
	check_string_as_dir(".", 2, "./");
	check_string_as_dir("abcd", 3, "abcd");
	check_string_as_dir("abcd", 4, "abcd");
	check_string_as_dir("abcd", 5, "abcd/");
}

void test_core_path__9(void)
{
	git_path a = GIT_PATH_INIT_STR("foo");
	git_path b = GIT_PATH_INIT_STR("bar");;

	cl_assert_strequal("foo", a.data);
	cl_assert_strequal("bar", b.data);
	git_path_swap(&a, &b);
	cl_assert_strequal("bar", a.data);
	cl_assert_strequal("foo", b.data);

	git_path_free(&a);
	git_path_free(&b);
}

void test_core_path__10(void)
{
	git_path a = GIT_PATH_INIT_STR("foo");
	char *b = NULL;

	cl_assert_strequal("foo", a.data);
	b = git_path_take_data(&a);
	cl_assert_strequal("foo", b);
	cl_assert_strequal(NULL, a.data);

	git__free(b);

	b = git_path_take_data(&a);
	cl_assert_strequal(NULL, b);
	cl_assert_strequal(NULL, a.data);
}

void test_core_path__11(void)
{
	const char *str_test = "this is a test";
	const char *str_8192 = TESTSTR_8192;
	git_path a = GIT_PATH_INIT;

	cl_assert_strequal(NULL, a.data);

	cl_git_pass(git_path_strcpy(&a, str_test));
	cl_assert_strequal(str_test, a.data);

	cl_git_pass(git_path_strcpy(&a, str_8192));
	cl_assert_strequal(str_8192, a.data);

	cl_git_pass(git_path_strcpy(&a, NULL));
	cl_assert_strequal(NULL, a.data);

	cl_git_pass(git_path_strcpy(&a, str_test));
	cl_assert_strequal(str_test, a.data);

	git_path_free(&a);
}
