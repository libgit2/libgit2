#include "clay_libgit2.h"
#include <fileops.h>

static void
check_dirname(const char *A, const char *B)
{
	git_path dir = GIT_PATH_INIT;
	char *dir2;

	cl_assert(git_path_dirname_r(&dir, A) >= 0);
	cl_assert(strcmp(dir.data, B) == 0);
	git__path_free(&dir);

	cl_assert((dir2 = git_path_dirname(A)) != NULL);
	cl_assert(strcmp(dir2, B) == 0);
	git__free(dir2);
}

static void
check_basename(const char *A, const char *B)
{
	git_path base = GIT_PATH_INIT;
	char *base2;

	cl_assert(git_path_basename_r(&base, A) >= 0);
	cl_assert(strcmp(base.data, B) == 0);
	git__path_free(&base);

	cl_assert((base2 = git_path_basename(A)) != NULL);
	cl_assert(strcmp(base2, B) == 0);
	git__free(base2);
}

static void
check_topdir(const char *A, const char *B)
{
	const char *dir;

	cl_assert((dir = git_path_topdir(A)) != NULL);
	cl_assert(strcmp(dir, B) == 0);
}

static void
check_joinpath(const char *path_a, const char *path_b, const char *expected_path)
{
	char joined_path[GIT_PATH_MAX];

	git_path_join(joined_path, path_a, path_b);
	cl_assert(strcmp(joined_path, expected_path) == 0);
}

static void
check_joinpath_n(
	const char *path_a,
	const char *path_b,
	const char *path_c,
	const char *path_d,
	const char *expected_path)
{
	char joined_path[GIT_PATH_MAX];

	git_path_join_n(joined_path, 4, path_a, path_b, path_c, path_d);
	cl_assert(strcmp(joined_path, expected_path) == 0);
}

static void
check_path_append(
	const char* path_a,
	const char* path_b,
	const char* expected_path,
	size_t expected_size)
{
	git_path tgt = GIT_PATH_INIT_STR(path_a);

	git__path_strcat(&tgt, path_b);

	if (!expected_path) {
		cl_assert(!tgt.data);
	} else {
		cl_assert(strcmp(tgt.data, expected_path) == 0);
	}

	cl_assert(tgt.size == expected_size);
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

	git__path_strncat(&tgt, path_b, len);

	if (!expected_path) {
		cl_assert(!tgt.data);
	} else {
		cl_assert(strcmp(tgt.data, expected_path) == 0);
	}

	cl_assert(tgt.size == expected_size);
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
	check_path_append(NULL, "", "", 1);
	check_path_append("", NULL, "", 1);
	check_path_append("", "", "", 1);
	check_path_append("a", NULL, "a", 2);
	check_path_append(NULL, "a", "a", 2);
	check_path_append("", "a", "a", 2);
	check_path_append("a", "", "a", 2);
	check_path_append("a", "b", "ab", 3);
	check_path_append(str_4096, NULL, str_4096, 4097);
	check_path_append(str_4096, "", str_4096, 4097);
	check_path_append(str_4096, str_4096, str_8192, 8193);

	check_path_strncat(NULL, NULL, 0, NULL, 0);
	check_path_strncat(NULL, NULL, 1, NULL, 0);
	check_path_strncat(NULL, "a", 0, "", 1);
	check_path_strncat(NULL, "a", 1, "a", 2);
	check_path_strncat(NULL, "a", 2, "a", 2);
	check_path_strncat(NULL, "a", 3, "a", 2);
	check_path_strncat(NULL, "ab", 0, "", 1);
	check_path_strncat(NULL, "ab", 1, "a", 2);
	check_path_strncat(NULL, "ab", 2, "ab", 3);
	check_path_strncat(NULL, "ab", 3, "ab", 3);
	check_path_strncat("ab", "cd", 0, "ab", 3);
	check_path_strncat("ab", "cd", 1, "abc", 4);
	check_path_strncat("ab", "cd", 2, "abcd", 5);
	check_path_strncat("ab", "cd", 3, "abcd", 5);
	check_path_strncat("ab", "cd", 4, "abcd", 5);
}
