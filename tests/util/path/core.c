#include "clar_libgit2.h"
#include "futils.h"
#include "fs_path.h"

#ifndef GIT_WIN32
# include <unistd.h>
#endif

static char *path_save;

void test_path_core__initialize(void)
{
	path_save = cl_getenv("PATH");
}

void test_path_core__cleanup(void)
{
	cl_setenv("PATH", path_save);
	git__free(path_save);
	path_save = NULL;
}

static void
check_dirname(const char *A, const char *B)
{
	git_str dir = GIT_STR_INIT;
	char *dir2;

	cl_assert(git_fs_path_dirname_r(&dir, A) >= 0);
	cl_assert_equal_s(B, dir.ptr);
	git_str_dispose(&dir);

	cl_assert((dir2 = git_fs_path_dirname(A)) != NULL);
	cl_assert_equal_s(B, dir2);
	git__free(dir2);
}

static void
check_basename(const char *A, const char *B)
{
	git_str base = GIT_STR_INIT;
	char *base2;

	cl_assert(git_fs_path_basename_r(&base, A) >= 0);
	cl_assert_equal_s(B, base.ptr);
	git_str_dispose(&base);

	cl_assert((base2 = git_fs_path_basename(A)) != NULL);
	cl_assert_equal_s(B, base2);
	git__free(base2);
}

static void
check_joinpath(const char *path_a, const char *path_b, const char *expected_path)
{
	git_str joined_path = GIT_STR_INIT;

	cl_git_pass(git_str_joinpath(&joined_path, path_a, path_b));
	cl_assert_equal_s(expected_path, joined_path.ptr);

	git_str_dispose(&joined_path);
}

static void
check_joinpath_n(
	const char *path_a,
	const char *path_b,
	const char *path_c,
	const char *path_d,
	const char *expected_path)
{
	git_str joined_path = GIT_STR_INIT;

	cl_git_pass(git_str_join_n(&joined_path, '/', 4,
							   path_a, path_b, path_c, path_d));
	cl_assert_equal_s(expected_path, joined_path.ptr);

	git_str_dispose(&joined_path);
}

static void check_setenv(const char* name, const char* value)
{
    char* check;

    cl_git_pass(cl_setenv(name, value));
    check = cl_getenv(name);

    if (value)
	cl_assert_equal_s(value, check);
    else
	cl_assert(check == NULL);

    git__free(check);
}

/* get the dirname of a path */
void test_path_core__00_dirname(void)
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

#ifdef GIT_WIN32
	check_dirname("C:/", "C:/");
	check_dirname("C:", "C:/");
	check_dirname("C:/path/", "C:/");
	check_dirname("C:/path", "C:/");
	check_dirname("//computername/", "//computername/");
	check_dirname("//computername", "//computername/");
	check_dirname("//computername/path/", "//computername/");
	check_dirname("//computername/path", "//computername/");
	check_dirname("//computername/sub/path/", "//computername/sub");
	check_dirname("//computername/sub/path", "//computername/sub");
#endif
}

/* get the base name of a path */
void test_path_core__01_basename(void)
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

/* properly join path components */
void test_path_core__05_joins(void)
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
void test_path_core__06_long_joins(void)
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
	git_str tgt = GIT_STR_INIT;

	git_str_sets(&tgt, path);
	cl_git_pass(git_fs_path_to_dir(&tgt));
	cl_assert_equal_s(expected, tgt.ptr);

	git_str_dispose(&tgt);
}

static void
check_string_to_dir(
	const char* path,
	size_t      maxlen,
    const char* expected)
{
	size_t len = strlen(path);
	char *buf = git__malloc(len + 2);
	cl_assert(buf);

	strncpy(buf, path, len + 2);

	git_fs_path_string_to_dir(buf, maxlen);

	cl_assert_equal_s(expected, buf);

	git__free(buf);
}

/* convert paths to dirs */
void test_path_core__07_path_to_dir(void)
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
void test_path_core__08_self_join(void)
{
	git_str path = GIT_STR_INIT;
	size_t asize = 0;

	asize = path.asize;
	cl_git_pass(git_str_sets(&path, "/foo"));
	cl_assert_equal_s(path.ptr, "/foo");
	cl_assert(asize < path.asize);

	asize = path.asize;
	cl_git_pass(git_str_joinpath(&path, path.ptr, "this is a new string"));
	cl_assert_equal_s(path.ptr, "/foo/this is a new string");
	cl_assert(asize < path.asize);

	asize = path.asize;
	cl_git_pass(git_str_joinpath(&path, path.ptr, "/grow the buffer, grow the buffer, grow the buffer"));
	cl_assert_equal_s(path.ptr, "/foo/this is a new string/grow the buffer, grow the buffer, grow the buffer");
	cl_assert(asize < path.asize);

	git_str_dispose(&path);
	cl_git_pass(git_str_sets(&path, "/foo/bar"));

	cl_git_pass(git_str_joinpath(&path, path.ptr + 4, "baz"));
	cl_assert_equal_s(path.ptr, "/bar/baz");

	asize = path.asize;
	cl_git_pass(git_str_joinpath(&path, path.ptr + 4, "somethinglongenoughtorealloc"));
	cl_assert_equal_s(path.ptr, "/baz/somethinglongenoughtorealloc");
	cl_assert(asize < path.asize);

	git_str_dispose(&path);
}

static void check_percent_decoding(const char *expected_result, const char *input)
{
	git_str buf = GIT_STR_INIT;

	cl_git_pass(git__percent_decode(&buf, input));
	cl_assert_equal_s(expected_result, git_str_cstr(&buf));

	git_str_dispose(&buf);
}

void test_path_core__09_percent_decode(void)
{
	check_percent_decoding("abcd", "abcd");
	check_percent_decoding("a2%", "a2%");
	check_percent_decoding("a2%3", "a2%3");
	check_percent_decoding("a2%%3", "a2%%3");
	check_percent_decoding("a2%3z", "a2%3z");
	check_percent_decoding("a,", "a%2c");
	check_percent_decoding("a21", "a2%31");
	check_percent_decoding("a2%1", "a2%%31");
	check_percent_decoding("a bc ", "a%20bc%20");
	check_percent_decoding("Vicent Mart" "\355", "Vicent%20Mart%ED");
}

static void check_fromurl(const char *expected_result, const char *input, int should_fail)
{
	git_str buf = GIT_STR_INIT;

	assert(should_fail || expected_result);

	if (!should_fail) {
		cl_git_pass(git_fs_path_fromurl(&buf, input));
		cl_assert_equal_s(expected_result, git_str_cstr(&buf));
	} else
		cl_git_fail(git_fs_path_fromurl(&buf, input));

	git_str_dispose(&buf);
}

#ifdef GIT_WIN32
#define ABS_PATH_MARKER ""
#else
#define ABS_PATH_MARKER "/"
#endif

void test_path_core__10_fromurl(void)
{
	/* Failing cases */
	check_fromurl(NULL, "a", 1);
	check_fromurl(NULL, "http:///c:/Temp%20folder/note.txt", 1);
	check_fromurl(NULL, "file://c:/Temp%20folder/note.txt", 1);
	check_fromurl(NULL, "file:////c:/Temp%20folder/note.txt", 1);
	check_fromurl(NULL, "file:///", 1);
	check_fromurl(NULL, "file:////", 1);
	check_fromurl(NULL, "file://servername/c:/Temp%20folder/note.txt", 1);

	/* Passing cases */
	check_fromurl(ABS_PATH_MARKER "c:/Temp folder/note.txt", "file:///c:/Temp%20folder/note.txt", 0);
	check_fromurl(ABS_PATH_MARKER "c:/Temp folder/note.txt", "file://localhost/c:/Temp%20folder/note.txt", 0);
	check_fromurl(ABS_PATH_MARKER "c:/Temp+folder/note.txt", "file:///c:/Temp+folder/note.txt", 0);
	check_fromurl(ABS_PATH_MARKER "a", "file:///a", 0);
}

typedef struct {
	int expect_idx;
	int cancel_after;
	char **expect;
} check_walkup_info;

#define CANCEL_VALUE 1234

static int check_one_walkup_step(void *ref, const char *path)
{
	check_walkup_info *info = (check_walkup_info *)ref;

	if (!info->cancel_after) {
		cl_assert_equal_s(info->expect[info->expect_idx], "[CANCEL]");
		return CANCEL_VALUE;
	}
	info->cancel_after--;

	cl_assert(info->expect[info->expect_idx] != NULL);
	cl_assert_equal_s(info->expect[info->expect_idx], path);
	info->expect_idx++;

	return 0;
}

void test_path_core__11_walkup(void)
{
	git_str p = GIT_STR_INIT;

	char *expect[] = {
		/*  1 */ "/a/b/c/d/e/", "/a/b/c/d/", "/a/b/c/", "/a/b/", "/a/", "/", NULL,
		/*  2 */ "/a/b/c/d/e", "/a/b/c/d/", "/a/b/c/", "/a/b/", "/a/", "/", NULL,
		/*  3 */ "/a/b/c/d/e", "/a/b/c/d/", "/a/b/c/", "/a/b/", "/a/", "/", NULL,
		/*  4 */ "/a/b/c/d/e", "/a/b/c/d/", "/a/b/c/", "/a/b/", "/a/", "/", NULL,
		/*  5 */ "/a/b/c/d/e", "/a/b/c/d/", "/a/b/c/", "/a/b/", NULL,
		/*  6 */ "/a/b/c/d/e", "/a/b/c/d/", "/a/b/c/", "/a/b/", NULL,
		/*  7 */ "this_is_a_path", "", NULL,
		/*  8 */ "this_is_a_path/", "", NULL,
		/*  9 */ "///a///b///c///d///e///", "///a///b///c///d///", "///a///b///c///", "///a///b///", "///a///", "///", NULL,
		/* 10 */ "a/b/c/", "a/b/", "a/", "", NULL,
		/* 11 */ "a/b/c", "a/b/", "a/", "", NULL,
		/* 12 */ "a/b/c/", "a/b/", "a/", NULL,
		/* 13 */ "", NULL,
		/* 14 */ "/", NULL,
		/* 15 */ NULL
	};

	char *root[] = {
		/*  1 */ NULL,
		/*  2 */ NULL,
		/*  3 */ "/",
		/*  4 */ "",
		/*  5 */ "/a/b",
		/*  6 */ "/a/b/",
		/*  7 */ NULL,
		/*  8 */ NULL,
		/*  9 */ NULL,
		/* 10 */ NULL,
		/* 11 */ NULL,
		/* 12 */ "a/",
		/* 13 */ NULL,
		/* 14 */ NULL,
	};

	int i, j;
	check_walkup_info info;

	info.expect = expect;
	info.cancel_after = -1;

	for (i = 0, j = 0; expect[i] != NULL; i++, j++) {

		git_str_sets(&p, expect[i]);

		info.expect_idx = i;
		cl_git_pass(
			git_fs_path_walk_up(&p, root[j], check_one_walkup_step, &info)
		);

		cl_assert_equal_s(p.ptr, expect[i]);
		cl_assert(expect[info.expect_idx] == NULL);
		i = info.expect_idx;
	}

	git_str_dispose(&p);
}

void test_path_core__11a_walkup_cancel(void)
{
	git_str p = GIT_STR_INIT;
	int cancel[] = { 3, 2, 1, 0 };
	char *expect[] = {
		"/a/b/c/d/e/", "/a/b/c/d/", "/a/b/c/", "[CANCEL]", NULL,
		"/a/b/c/d/e", "/a/b/c/d/", "[CANCEL]", NULL,
		"/a/b/c/d/e", "[CANCEL]", NULL,
		"[CANCEL]", NULL,
		NULL
	};
	char *root[] = { NULL, NULL, "/", "", NULL };
	int i, j;
	check_walkup_info info;

	info.expect = expect;

	for (i = 0, j = 0; expect[i] != NULL; i++, j++) {

		git_str_sets(&p, expect[i]);

		info.cancel_after = cancel[j];
		info.expect_idx = i;

		cl_assert_equal_i(
			CANCEL_VALUE,
			git_fs_path_walk_up(&p, root[j], check_one_walkup_step, &info)
		);

		/* skip to next run of expectations */
		while (expect[i] != NULL) i++;
	}

	git_str_dispose(&p);
}

void test_path_core__12_offset_to_path_root(void)
{
	cl_assert(git_fs_path_root("non/rooted/path") == -1);
	cl_assert(git_fs_path_root("/rooted/path") == 0);

#ifdef GIT_WIN32
	/* Windows specific tests */
	cl_assert(git_fs_path_root("C:non/rooted/path") == -1);
	cl_assert(git_fs_path_root("C:/rooted/path") == 2);
	cl_assert(git_fs_path_root("//computername/sharefolder/resource") == 14);
	cl_assert(git_fs_path_root("//computername/sharefolder") == 14);
	cl_assert(git_fs_path_root("//computername") == -1);
#endif
}

#define NON_EXISTING_FILEPATH "i_hope_i_do_not_exist"

void test_path_core__13_cannot_prettify_a_non_existing_file(void)
{
	git_str p = GIT_STR_INIT;

	cl_assert_equal_b(git_fs_path_exists(NON_EXISTING_FILEPATH), false);
	cl_assert_equal_i(GIT_ENOTFOUND, git_fs_path_prettify(&p, NON_EXISTING_FILEPATH, NULL));
	cl_assert_equal_i(GIT_ENOTFOUND, git_fs_path_prettify(&p, NON_EXISTING_FILEPATH "/so-do-i", NULL));

	git_str_dispose(&p);
}

void test_path_core__14_apply_relative(void)
{
	git_str p = GIT_STR_INIT;

	cl_git_pass(git_str_sets(&p, "/this/is/a/base"));

	cl_git_pass(git_fs_path_apply_relative(&p, "../test"));
	cl_assert_equal_s("/this/is/a/test", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "../../the/./end"));
	cl_assert_equal_s("/this/is/the/end", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "./of/this/../the/string"));
	cl_assert_equal_s("/this/is/the/end/of/the/string", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "../../../../../.."));
	cl_assert_equal_s("/this/", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "../"));
	cl_assert_equal_s("/", p.ptr);

	cl_git_fail(git_fs_path_apply_relative(&p, "../../.."));


	cl_git_pass(git_str_sets(&p, "d:/another/test"));

	cl_git_pass(git_fs_path_apply_relative(&p, "../.."));
	cl_assert_equal_s("d:/", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "from/here/to/../and/./back/."));
	cl_assert_equal_s("d:/from/here/and/back/", p.ptr);


	cl_git_pass(git_str_sets(&p, "https://my.url.com/test.git"));

	cl_git_pass(git_fs_path_apply_relative(&p, "../another.git"));
	cl_assert_equal_s("https://my.url.com/another.git", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "../full/path/url.patch"));
	cl_assert_equal_s("https://my.url.com/full/path/url.patch", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, ".."));
	cl_assert_equal_s("https://my.url.com/full/path/", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "../../../"));
	cl_assert_equal_s("https://", p.ptr);


	cl_git_pass(git_str_sets(&p, "../../this/is/relative"));

	cl_git_pass(git_fs_path_apply_relative(&p, "../../preserves/the/prefix"));
	cl_assert_equal_s("../../this/preserves/the/prefix", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "../../../../that"));
	cl_assert_equal_s("../../that", p.ptr);

	cl_git_pass(git_fs_path_apply_relative(&p, "../there"));
	cl_assert_equal_s("../../there", p.ptr);
	git_str_dispose(&p);
}

static void assert_resolve_relative(
	git_str *buf, const char *expected, const char *path)
{
	cl_git_pass(git_str_sets(buf, path));
	cl_git_pass(git_fs_path_resolve_relative(buf, 0));
	cl_assert_equal_s(expected, buf->ptr);
}

void test_path_core__15_resolve_relative(void)
{
	git_str buf = GIT_STR_INIT;

	assert_resolve_relative(&buf, "", "");
	assert_resolve_relative(&buf, "", ".");
	assert_resolve_relative(&buf, "", "./");
	assert_resolve_relative(&buf, "..", "..");
	assert_resolve_relative(&buf, "../", "../");
	assert_resolve_relative(&buf, "..", "./..");
	assert_resolve_relative(&buf, "../", "./../");
	assert_resolve_relative(&buf, "../", "../.");
	assert_resolve_relative(&buf, "../", ".././");
	assert_resolve_relative(&buf, "../..", "../..");
	assert_resolve_relative(&buf, "../../", "../../");

	assert_resolve_relative(&buf, "/", "/");
	assert_resolve_relative(&buf, "/", "/.");

	assert_resolve_relative(&buf, "", "a/..");
	assert_resolve_relative(&buf, "", "a/../");
	assert_resolve_relative(&buf, "", "a/../.");

	assert_resolve_relative(&buf, "/a", "/a");
	assert_resolve_relative(&buf, "/a/", "/a/.");
	assert_resolve_relative(&buf, "/", "/a/../");
	assert_resolve_relative(&buf, "/", "/a/../.");
	assert_resolve_relative(&buf, "/", "/a/.././");

	assert_resolve_relative(&buf, "a", "a");
	assert_resolve_relative(&buf, "a/", "a/");
	assert_resolve_relative(&buf, "a/", "a/.");
	assert_resolve_relative(&buf, "a/", "a/./");

	assert_resolve_relative(&buf, "a/b", "a//b");
	assert_resolve_relative(&buf, "a/b/c", "a/b/c");
	assert_resolve_relative(&buf, "b/c", "./b/c");
	assert_resolve_relative(&buf, "a/c", "a/./c");
	assert_resolve_relative(&buf, "a/b/", "a/b/.");

	assert_resolve_relative(&buf, "/a/b/c", "///a/b/c");
	assert_resolve_relative(&buf, "/", "////");
	assert_resolve_relative(&buf, "/a", "///a");
	assert_resolve_relative(&buf, "/", "///.");
	assert_resolve_relative(&buf, "/", "///a/..");

	assert_resolve_relative(&buf, "../../path", "../../test//../././path");
	assert_resolve_relative(&buf, "../d", "a/b/../../../c/../d");

	cl_git_pass(git_str_sets(&buf, "/.."));
	cl_git_fail(git_fs_path_resolve_relative(&buf, 0));

	cl_git_pass(git_str_sets(&buf, "/./.."));
	cl_git_fail(git_fs_path_resolve_relative(&buf, 0));

	cl_git_pass(git_str_sets(&buf, "/.//.."));
	cl_git_fail(git_fs_path_resolve_relative(&buf, 0));

	cl_git_pass(git_str_sets(&buf, "/../."));
	cl_git_fail(git_fs_path_resolve_relative(&buf, 0));

	cl_git_pass(git_str_sets(&buf, "/../.././../a"));
	cl_git_fail(git_fs_path_resolve_relative(&buf, 0));

	cl_git_pass(git_str_sets(&buf, "////.."));
	cl_git_fail(git_fs_path_resolve_relative(&buf, 0));

	/* things that start with Windows network paths */
#ifdef GIT_WIN32
	assert_resolve_relative(&buf, "//a/b/c", "//a/b/c");
	assert_resolve_relative(&buf, "//a/", "//a/b/..");
	assert_resolve_relative(&buf, "//a/b/c", "//a/Q/../b/x/y/../../c");

	cl_git_pass(git_str_sets(&buf, "//a/b/../.."));
	cl_git_fail(git_fs_path_resolve_relative(&buf, 0));
#else
	assert_resolve_relative(&buf, "/a/b/c", "//a/b/c");
	assert_resolve_relative(&buf, "/a/", "//a/b/..");
	assert_resolve_relative(&buf, "/a/b/c", "//a/Q/../b/x/y/../../c");
	assert_resolve_relative(&buf, "/", "//a/b/../..");
#endif

	git_str_dispose(&buf);
}

#define assert_common_dirlen(i, p, q) \
	cl_assert_equal_i((i), git_fs_path_common_dirlen((p), (q)));

void test_path_core__16_resolve_relative(void)
{
	assert_common_dirlen(0, "", "");
	assert_common_dirlen(0, "", "bar.txt");
	assert_common_dirlen(0, "foo.txt", "bar.txt");
	assert_common_dirlen(0, "foo.txt", "");
	assert_common_dirlen(0, "foo/bar.txt", "bar/foo.txt");
	assert_common_dirlen(0, "foo/bar.txt", "../foo.txt");

	assert_common_dirlen(1, "/one.txt", "/two.txt");
	assert_common_dirlen(4, "foo/one.txt", "foo/two.txt");
	assert_common_dirlen(5, "/foo/one.txt", "/foo/two.txt");

	assert_common_dirlen(6, "a/b/c/foo.txt", "a/b/c/d/e/bar.txt");
	assert_common_dirlen(7, "/a/b/c/foo.txt", "/a/b/c/d/e/bar.txt");
}

static void fix_path(git_str *s)
{
#ifndef GIT_WIN32
	GIT_UNUSED(s);
#else
	char* c;

	for (c = s->ptr; *c; c++) {
		if (*c == '/')
			*c = '\\';
	}
#endif
}

void test_path_core__find_exe_in_path(void)
{
	char *orig_path;
	git_str sandbox_path = GIT_STR_INIT;
	git_str new_path = GIT_STR_INIT, full_path = GIT_STR_INIT,
	        dummy_path = GIT_STR_INIT;

#ifdef GIT_WIN32
	static const char *bogus_path_1 = "c:\\does\\not\\exist\\";
	static const char *bogus_path_2 = "e:\\non\\existent";
#else
	static const char *bogus_path_1 = "/this/path/does/not/exist/";
	static const char *bogus_path_2 = "/non/existent";
#endif

	orig_path = cl_getenv("PATH");

	git_str_puts(&sandbox_path, clar_sandbox_path());
	git_str_joinpath(&dummy_path, sandbox_path.ptr, "dummmmmmmy_libgit2_file");
	cl_git_rewritefile(dummy_path.ptr, "this is a dummy file");

	fix_path(&sandbox_path);
	fix_path(&dummy_path);

	cl_git_pass(git_str_printf(&new_path, "%s%c%s%c%s%c%s",
		bogus_path_1, GIT_PATH_LIST_SEPARATOR,
		orig_path, GIT_PATH_LIST_SEPARATOR,
		sandbox_path.ptr, GIT_PATH_LIST_SEPARATOR,
		bogus_path_2));

	check_setenv("PATH", new_path.ptr);

	cl_git_fail_with(GIT_ENOTFOUND, git_fs_path_find_executable(&full_path, "this_file_does_not_exist"));
	cl_git_pass(git_fs_path_find_executable(&full_path, "dummmmmmmy_libgit2_file"));

	cl_assert_equal_s(full_path.ptr, dummy_path.ptr);

	git_str_dispose(&full_path);
	git_str_dispose(&new_path);
	git_str_dispose(&dummy_path);
	git_str_dispose(&sandbox_path);
	git__free(orig_path);
}

void test_path_core__validate_current_user_ownership(void)
{
	bool is_cur;

	cl_must_pass(p_mkdir("testdir", 0777));
	cl_git_pass(git_fs_path_owner_is_current_user(&is_cur, "testdir"));
	cl_assert_equal_i(is_cur, 1);

	cl_git_rewritefile("testfile", "This is a test file.");
	cl_git_pass(git_fs_path_owner_is_current_user(&is_cur, "testfile"));
	cl_assert_equal_i(is_cur, 1);

#ifdef GIT_WIN32
	cl_git_pass(git_fs_path_owner_is_current_user(&is_cur, "C:\\"));
	cl_assert_equal_i(is_cur, 0);

	cl_git_fail(git_fs_path_owner_is_current_user(&is_cur, "c:\\path\\does\\not\\exist"));
#else
	cl_git_pass(git_fs_path_owner_is_current_user(&is_cur, "/"));
	cl_assert_equal_i(is_cur, (geteuid() == 0));

	cl_git_fail(git_fs_path_owner_is_current_user(&is_cur, "/path/does/not/exist"));
#endif
}

void test_path_core__dirlen(void)
{
	cl_assert_equal_sz(13, git_fs_path_dirlen("/foo/bar/asdf"));
	cl_assert_equal_sz(13, git_fs_path_dirlen("/foo/bar/asdf/"));
	cl_assert_equal_sz(13, git_fs_path_dirlen("/foo/bar/asdf//"));
	cl_assert_equal_sz(3, git_fs_path_dirlen("foo////"));
	cl_assert_equal_sz(3, git_fs_path_dirlen("foo"));
	cl_assert_equal_sz(1, git_fs_path_dirlen("/"));
	cl_assert_equal_sz(1, git_fs_path_dirlen("////"));
	cl_assert_equal_sz(0, git_fs_path_dirlen(""));
}

static void test_make_relative(
	const char *expected_path,
	const char *path,
	const char *parent,
	int expected_status)
{
	git_str buf = GIT_STR_INIT;
	git_str_puts(&buf, path);
	cl_assert_equal_i(expected_status, git_fs_path_make_relative(&buf, parent));
	cl_assert_equal_s(expected_path, buf.ptr);
	git_str_dispose(&buf);
}

void test_path_core__make_relative(void)
{
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

void test_path_core__isvalid_standard(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar/file.txt", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar/.file", 0));
}

/* Ensure that `is_valid_str` only reads str->size bytes */
void test_path_core__isvalid_standard_str(void)
{
	git_str str = GIT_STR_INIT_CONST("foo/bar//zap", 0);
	unsigned int flags = GIT_FS_PATH_REJECT_EMPTY_COMPONENT;

	str.size = 0;
	cl_assert_equal_b(false, git_fs_path_str_is_valid(&str, flags));

	str.size = 3;
	cl_assert_equal_b(true, git_fs_path_str_is_valid(&str, flags));

	str.size = 4;
	cl_assert_equal_b(false, git_fs_path_str_is_valid(&str, flags));

	str.size = 5;
	cl_assert_equal_b(true, git_fs_path_str_is_valid(&str, flags));

	str.size = 7;
	cl_assert_equal_b(true, git_fs_path_str_is_valid(&str, flags));

	str.size = 8;
	cl_assert_equal_b(false, git_fs_path_str_is_valid(&str, flags));

	str.size = strlen(str.ptr);
	cl_assert_equal_b(false, git_fs_path_str_is_valid(&str, flags));
}

void test_path_core__isvalid_empty_dir_component(void)
{
	unsigned int flags = GIT_FS_PATH_REJECT_EMPTY_COMPONENT;

	/* empty component */
	cl_assert_equal_b(true, git_fs_path_is_valid("foo//bar", 0));

	/* leading slash */
	cl_assert_equal_b(true, git_fs_path_is_valid("/", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("/foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("/foo/bar", 0));

	/* trailing slash */
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar/", 0));


	/* empty component */
	cl_assert_equal_b(false, git_fs_path_is_valid("foo//bar", flags));

	/* leading slash */
	cl_assert_equal_b(false, git_fs_path_is_valid("/", flags));
	cl_assert_equal_b(false, git_fs_path_is_valid("/foo", flags));
	cl_assert_equal_b(false, git_fs_path_is_valid("/foo/bar", flags));

	/* trailing slash */
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/", flags));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/bar/", flags));
}

void test_path_core__isvalid_dot_and_dotdot(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid(".", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("./foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/.", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("./foo", 0));

	cl_assert_equal_b(true, git_fs_path_is_valid("..", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("../foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/..", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("../foo", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid(".", GIT_FS_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_fs_path_is_valid("./foo", GIT_FS_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/.", GIT_FS_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_fs_path_is_valid("./foo", GIT_FS_PATH_REJECT_TRAVERSAL));

	cl_assert_equal_b(false, git_fs_path_is_valid("..", GIT_FS_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_fs_path_is_valid("../foo", GIT_FS_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/..", GIT_FS_PATH_REJECT_TRAVERSAL));
	cl_assert_equal_b(false, git_fs_path_is_valid("../foo", GIT_FS_PATH_REJECT_TRAVERSAL));
}

void test_path_core__isvalid_backslash(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("foo\\file.txt", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar\\file.txt", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar\\", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid("foo\\file.txt", GIT_FS_PATH_REJECT_BACKSLASH));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/bar\\file.txt", GIT_FS_PATH_REJECT_BACKSLASH));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/bar\\", GIT_FS_PATH_REJECT_BACKSLASH));
}

void test_path_core__isvalid_trailing_dot(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("foo.", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo...", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar.", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo./bar", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid("foo.", GIT_FS_PATH_REJECT_TRAILING_DOT));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo...", GIT_FS_PATH_REJECT_TRAILING_DOT));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/bar.", GIT_FS_PATH_REJECT_TRAILING_DOT));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo./bar", GIT_FS_PATH_REJECT_TRAILING_DOT));
}

void test_path_core__isvalid_trailing_space(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("foo ", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo   ", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar ", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid(" ", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo /bar", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid("foo ", GIT_FS_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo   ", GIT_FS_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/bar ", GIT_FS_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_fs_path_is_valid(" ", GIT_FS_PATH_REJECT_TRAILING_SPACE));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo /bar", GIT_FS_PATH_REJECT_TRAILING_SPACE));
}

void test_path_core__isvalid_trailing_colon(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("foo:", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo/bar:", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid(":", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("foo:/bar", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid("foo:", GIT_FS_PATH_REJECT_TRAILING_COLON));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo/bar:", GIT_FS_PATH_REJECT_TRAILING_COLON));
	cl_assert_equal_b(false, git_fs_path_is_valid(":", GIT_FS_PATH_REJECT_TRAILING_COLON));
	cl_assert_equal_b(false, git_fs_path_is_valid("foo:/bar", GIT_FS_PATH_REJECT_TRAILING_COLON));
}

void test_path_core__isvalid_dos_paths(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("aux", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("aux.", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("aux:", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("aux.asdf", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("aux.asdf\\zippy", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("aux:asdf\\foobar", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("con", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("prn", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("nul", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid("aux", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("aux.", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("aux:", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("aux.asdf", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("aux.asdf\\zippy", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("aux:asdf\\foobar", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("con", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("prn", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("nul", GIT_FS_PATH_REJECT_DOS_PATHS));

	cl_assert_equal_b(true, git_fs_path_is_valid("aux1", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("aux1", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("auxn", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("aux\\foo", GIT_FS_PATH_REJECT_DOS_PATHS));
}

void test_path_core__isvalid_dos_paths_withnum(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("com1", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com1.", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com1:", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com1.asdf", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com1.asdf\\zippy", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com1:asdf\\foobar", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com1\\foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("lpt1", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid("com1", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("com1.", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("com1:", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("com1.asdf", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("com1.asdf\\zippy", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("com1:asdf\\foobar", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("com1/foo", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(false, git_fs_path_is_valid("lpt1", GIT_FS_PATH_REJECT_DOS_PATHS));

	cl_assert_equal_b(true, git_fs_path_is_valid("com0", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com0", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("com10", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("com10", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("comn", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("com1\\foo", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("lpt0", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("lpt10", GIT_FS_PATH_REJECT_DOS_PATHS));
	cl_assert_equal_b(true, git_fs_path_is_valid("lptn", GIT_FS_PATH_REJECT_DOS_PATHS));
}

void test_path_core__isvalid_nt_chars(void)
{
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf\001foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf\037bar", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf<bar", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf>foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf:foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf\"bar", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf|foo", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf?bar", 0));
	cl_assert_equal_b(true, git_fs_path_is_valid("asdf*bar", 0));

	cl_assert_equal_b(false, git_fs_path_is_valid("asdf\001foo", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf\037bar", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf<bar", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf>foo", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf:foo", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf\"bar", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf|foo", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf?bar", GIT_FS_PATH_REJECT_NT_CHARS));
	cl_assert_equal_b(false, git_fs_path_is_valid("asdf*bar", GIT_FS_PATH_REJECT_NT_CHARS));
}

static void test_join_unrooted(
	const char *expected_result,
	ssize_t expected_rootlen,
	const char *path,
	const char *base)
{
	git_str result = GIT_STR_INIT;
	ssize_t root_at;

	cl_git_pass(git_fs_path_join_unrooted(&result, path, base, &root_at));
	cl_assert_equal_s(expected_result, result.ptr);
	cl_assert_equal_i(expected_rootlen, root_at);

	git_str_dispose(&result);
}

void test_path_core__join_unrooted(void)
{
	git_str out = GIT_STR_INIT;

	test_join_unrooted("foo", 0, "foo", NULL);
	test_join_unrooted("foo/bar", 0, "foo/bar", NULL);

	/* Relative paths have base prepended */
	test_join_unrooted("/foo/bar", 4, "bar", "/foo");
	test_join_unrooted("/foo/bar/foobar", 4, "bar/foobar", "/foo");
	test_join_unrooted("c:/foo/bar/foobar", 6, "bar/foobar", "c:/foo");
	test_join_unrooted("c:/foo/bar/foobar", 10, "foobar", "c:/foo/bar");

	/* Absolute paths are not prepended with base */
	test_join_unrooted("/foo", 0, "/foo", "/asdf");
	test_join_unrooted("/foo/bar", 0, "/foo/bar", "/asdf");

	/* Drive letter is given as root length on Windows */
	test_join_unrooted("c:/foo", 2, "c:/foo", "c:/asdf");
	test_join_unrooted("c:/foo/bar", 2, "c:/foo/bar", "c:/asdf");

#ifdef GIT_WIN32
	/* Paths starting with '\\' are absolute */
	test_join_unrooted("\\bar", 0, "\\bar", "c:/foo/");
	test_join_unrooted("\\\\network\\bar", 9, "\\\\network\\bar", "c:/foo/");
#else
	/* Paths starting with '\\' are not absolute on non-Windows systems */
	test_join_unrooted("/foo/\\bar", 4, "\\bar", "/foo");
	test_join_unrooted("c:/foo/\\bar", 7, "\\bar", "c:/foo/");
#endif

	/* Base is returned when it's provided and is the prefix */
	test_join_unrooted("c:/foo/bar/foobar", 6, "c:/foo/bar/foobar", "c:/foo");
	test_join_unrooted("c:/foo/bar/foobar", 10, "c:/foo/bar/foobar", "c:/foo/bar");

	/* Trailing slash in the base is ignored */
	test_join_unrooted("c:/foo/bar/foobar", 6, "c:/foo/bar/foobar", "c:/foo/");

	git_str_dispose(&out);
}

void test_path_core__join_unrooted_respects_funny_windows_roots(void)
{
	test_join_unrooted("💩:/foo/bar/foobar", 9, "bar/foobar", "💩:/foo");
	test_join_unrooted("💩:/foo/bar/foobar", 13, "foobar", "💩:/foo/bar");
	test_join_unrooted("💩:/foo", 5, "💩:/foo", "💩:/asdf");
	test_join_unrooted("💩:/foo/bar", 5, "💩:/foo/bar", "💩:/asdf");
	test_join_unrooted("💩:/foo/bar/foobar", 9, "💩:/foo/bar/foobar", "💩:/foo");
	test_join_unrooted("💩:/foo/bar/foobar", 13, "💩:/foo/bar/foobar", "💩:/foo/bar");
	test_join_unrooted("💩:/foo/bar/foobar", 9, "💩:/foo/bar/foobar", "💩:/foo/");
}

void test_path_core__is_root(void)
{
	cl_assert_equal_b(true,  git_fs_path_is_root("/"));
	cl_assert_equal_b(false, git_fs_path_is_root("//"));
	cl_assert_equal_b(false, git_fs_path_is_root("foo/"));
	cl_assert_equal_b(false, git_fs_path_is_root("/foo/"));
	cl_assert_equal_b(false, git_fs_path_is_root("/foo"));
	cl_assert_equal_b(false, git_fs_path_is_root("\\"));

#ifdef GIT_WIN32
	cl_assert_equal_b(true,  git_fs_path_is_root("A:\\"));
	cl_assert_equal_b(false, git_fs_path_is_root("B:\\foo"));
	cl_assert_equal_b(false, git_fs_path_is_root("B:\\foo\\"));
	cl_assert_equal_b(true,  git_fs_path_is_root("C:\\"));
	cl_assert_equal_b(true,  git_fs_path_is_root("c:\\"));
	cl_assert_equal_b(true,  git_fs_path_is_root("z:\\"));
	cl_assert_equal_b(false, git_fs_path_is_root("z:\\\\"));
	cl_assert_equal_b(false, git_fs_path_is_root("\\\\localhost"));
	cl_assert_equal_b(false, git_fs_path_is_root("\\\\localhost\\"));
	cl_assert_equal_b(false, git_fs_path_is_root("\\\\localhost\\c$\\"));
	cl_assert_equal_b(false, git_fs_path_is_root("\\\\localhost\\c$\\Foo"));
	cl_assert_equal_b(false, git_fs_path_is_root("\\\\localhost\\c$\\Foo\\"));
	cl_assert_equal_b(false, git_fs_path_is_root("\\\\Volume\\12345\\Foo\\Bar.txt"));
#endif
}
