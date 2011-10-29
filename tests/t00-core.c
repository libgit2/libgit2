/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"

#include "vector.h"
#include "fileops.h"
#include "filebuf.h"

BEGIN_TEST(string0, "compare prefixes")
	must_be_true(git__prefixcmp("", "") == 0);
	must_be_true(git__prefixcmp("a", "") == 0);
	must_be_true(git__prefixcmp("", "a") < 0);
	must_be_true(git__prefixcmp("a", "b") < 0);
	must_be_true(git__prefixcmp("b", "a") > 0);
	must_be_true(git__prefixcmp("ab", "a") == 0);
	must_be_true(git__prefixcmp("ab", "ac") < 0);
	must_be_true(git__prefixcmp("ab", "aa") > 0);
END_TEST

BEGIN_TEST(string1, "compare suffixes")
	must_be_true(git__suffixcmp("", "") == 0);
	must_be_true(git__suffixcmp("a", "") == 0);
	must_be_true(git__suffixcmp("", "a") < 0);
	must_be_true(git__suffixcmp("a", "b") < 0);
	must_be_true(git__suffixcmp("b", "a") > 0);
	must_be_true(git__suffixcmp("ba", "a") == 0);
	must_be_true(git__suffixcmp("zaa", "ac") < 0);
	must_be_true(git__suffixcmp("zaz", "ac") > 0);
END_TEST


BEGIN_TEST(vector0, "initial size of 1 would cause writing past array bounds")
  git_vector x;
  int i;
  git_vector_init(&x, 1, NULL);
  for (i = 0; i < 10; ++i) {
    git_vector_insert(&x, (void*) 0xabc);
  }
  git_vector_free(&x);
END_TEST

BEGIN_TEST(vector1, "don't read past array bounds on remove()")
  git_vector x;
  // make initial capacity exact for our insertions.
  git_vector_init(&x, 3, NULL);
  git_vector_insert(&x, (void*) 0xabc);
  git_vector_insert(&x, (void*) 0xdef);
  git_vector_insert(&x, (void*) 0x123);

  git_vector_remove(&x, 0);  // used to read past array bounds.
  git_vector_free(&x);
END_TEST

static int test_cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

BEGIN_TEST(vector2, "remove duplicates")
	git_vector x;
	int *ptrs[2];

	ptrs[0] = git__malloc(sizeof(int));
	ptrs[1] = git__malloc(sizeof(int));

	*ptrs[0] = 2;
	*ptrs[1] = 1;

	must_pass(git_vector_init(&x, 5, test_cmp));
	must_pass(git_vector_insert(&x, ptrs[0]));
	must_pass(git_vector_insert(&x, ptrs[1]));
	must_pass(git_vector_insert(&x, ptrs[1]));
	must_pass(git_vector_insert(&x, ptrs[0]));
	must_pass(git_vector_insert(&x, ptrs[1]));
	must_be_true(x.length == 5);
	git_vector_uniq(&x);
	must_be_true(x.length == 2);
	git_vector_free(&x);

	git__free(ptrs[0]);
	git__free(ptrs[1]);
END_TEST


BEGIN_TEST(path0, "get the dirname of a path")
	char dir[64], *dir2;

#define DIRNAME_TEST(A, B) { \
	must_be_true(git_path_dirname_r(dir, sizeof(dir), A) >= 0); \
	must_be_true(strcmp(dir, B) == 0);				\
	must_be_true((dir2 = git_path_dirname(A)) != NULL);	\
	must_be_true(strcmp(dir2, B) == 0);				\
	git__free(dir2);										\
}

	DIRNAME_TEST(NULL, ".");
	DIRNAME_TEST("", ".");
	DIRNAME_TEST("a", ".");
	DIRNAME_TEST("/", "/");
	DIRNAME_TEST("/usr", "/");
	DIRNAME_TEST("/usr/", "/");
	DIRNAME_TEST("/usr/lib", "/usr");
	DIRNAME_TEST("/usr/lib/", "/usr");
	DIRNAME_TEST("/usr/lib//", "/usr");
	DIRNAME_TEST("usr/lib", "usr");
	DIRNAME_TEST("usr/lib/", "usr");
	DIRNAME_TEST("usr/lib//", "usr");
	DIRNAME_TEST(".git/", ".");

#undef DIRNAME_TEST

END_TEST

BEGIN_TEST(path1, "get the base name of a path")
	char base[64], *base2;

#define BASENAME_TEST(A, B) { \
	must_be_true(git_path_basename_r(base, sizeof(base), A) >= 0); \
	must_be_true(strcmp(base, B) == 0);					\
	must_be_true((base2 = git_path_basename(A)) != NULL);	\
	must_be_true(strcmp(base2, B) == 0);				\
	git__free(base2);										\
}

	BASENAME_TEST(NULL, ".");
	BASENAME_TEST("", ".");
	BASENAME_TEST("a", "a");
	BASENAME_TEST("/", "/");
	BASENAME_TEST("/usr", "usr");
	BASENAME_TEST("/usr/", "usr");
	BASENAME_TEST("/usr/lib", "lib");
	BASENAME_TEST("/usr/lib//", "lib");
	BASENAME_TEST("usr/lib", "lib");

#undef BASENAME_TEST

END_TEST

BEGIN_TEST(path2, "get the latest component in a path")
	const char *dir;

#define TOPDIR_TEST(A, B) { \
	must_be_true((dir = git_path_topdir(A)) != NULL);	\
	must_be_true(strcmp(dir, B) == 0);				\
}

	TOPDIR_TEST(".git/", ".git/");
	TOPDIR_TEST("/.git/", ".git/");
	TOPDIR_TEST("usr/local/.git/", ".git/");
	TOPDIR_TEST("./.git/", ".git/");
	TOPDIR_TEST("/usr/.git/", ".git/");
	TOPDIR_TEST("/", "/");
	TOPDIR_TEST("a/", "a/");

	must_be_true(git_path_topdir("/usr/.git") == NULL);
	must_be_true(git_path_topdir(".") == NULL);
	must_be_true(git_path_topdir("") == NULL);
	must_be_true(git_path_topdir("a") == NULL);

#undef TOPDIR_TEST
END_TEST

static int ensure_joinpath(const char *path_a, const char *path_b, const char *expected_path)
{
	char joined_path[GIT_PATH_MAX];
	git_path_join(joined_path, path_a, path_b);
	return strcmp(joined_path, expected_path) == 0 ? GIT_SUCCESS : GIT_ERROR;
}

BEGIN_TEST(path5, "properly join path components")
	must_pass(ensure_joinpath("", "", ""));
	must_pass(ensure_joinpath("", "a", "a"));
	must_pass(ensure_joinpath("", "/a", "/a"));
	must_pass(ensure_joinpath("a", "", "a/"));
	must_pass(ensure_joinpath("a", "/", "a/"));
	must_pass(ensure_joinpath("a", "b", "a/b"));
	must_pass(ensure_joinpath("/", "a", "/a"));
	must_pass(ensure_joinpath("/", "", "/"));
	must_pass(ensure_joinpath("/a", "/b", "/a/b"));
	must_pass(ensure_joinpath("/a", "/b/", "/a/b/"));
	must_pass(ensure_joinpath("/a/", "b/", "/a/b/"));
	must_pass(ensure_joinpath("/a/", "/b/", "/a/b/"));
END_TEST

static int ensure_joinpath_n(const char *path_a, const char *path_b, const char *path_c, const char *path_d, const char *expected_path)
{
	char joined_path[GIT_PATH_MAX];
	git_path_join_n(joined_path, 4, path_a, path_b, path_c, path_d);
	return strcmp(joined_path, expected_path) == 0 ? GIT_SUCCESS : GIT_ERROR;
}

BEGIN_TEST(path6, "properly join path components for more than one path")
	must_pass(ensure_joinpath_n("", "", "", "", ""));
	must_pass(ensure_joinpath_n("", "a", "", "", "a/"));
	must_pass(ensure_joinpath_n("a", "", "", "", "a/"));
	must_pass(ensure_joinpath_n("", "", "", "a", "a"));
	must_pass(ensure_joinpath_n("a", "b", "", "/c/d/", "a/b/c/d/"));
	must_pass(ensure_joinpath_n("a", "b", "", "/c/d", "a/b/c/d"));
END_TEST

typedef struct name_data {
	int  count;  /* return count */
	char *name;  /* filename     */
} name_data;

typedef struct walk_data {
	char *sub;        /* sub-directory name */
	name_data *names; /* name state data    */
} walk_data;


static char path_buffer[GIT_PATH_MAX];
static char *top_dir = "dir-walk";
static walk_data *state_loc;

static int error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	return -1;
}

static int setup(walk_data *d)
{
	name_data *n;

	if (p_mkdir(top_dir, 0777) < 0)
		return error("can't mkdir(\"%s\")", top_dir);

	if (p_chdir(top_dir) < 0)
		return error("can't chdir(\"%s\")", top_dir);

	if (strcmp(d->sub, ".") != 0)
		if (p_mkdir(d->sub, 0777) < 0)
			return error("can't mkdir(\"%s\")", d->sub);

	strcpy(path_buffer, d->sub);
	state_loc = d;

	for (n = d->names; n->name; n++) {
		git_file fd = p_creat(n->name, 0666);
		if (fd < 0)
			return GIT_ERROR;
		p_close(fd);
		n->count = 0;
	}

	return 0;
}

static int knockdown(walk_data *d)
{
	name_data *n;

	for (n = d->names; n->name; n++) {
		if (p_unlink(n->name) < 0)
			return error("can't unlink(\"%s\")", n->name);
	}

	if (strcmp(d->sub, ".") != 0)
		if (p_rmdir(d->sub) < 0)
			return error("can't rmdir(\"%s\")", d->sub);

	if (p_chdir("..") < 0)
		return error("can't chdir(\"..\")");

	if (p_rmdir(top_dir) < 0)
		return error("can't rmdir(\"%s\")", top_dir);

	return 0;
}

static int check_counts(walk_data *d)
{
	int ret = 0;
	name_data *n;

	for (n = d->names; n->name; n++) {
		if (n->count != 1)
			ret = error("count (%d, %s)", n->count, n->name);
	}
	return ret;
}

static int one_entry(void *state, char *path)
{
	walk_data *d = (walk_data *) state;
	name_data *n;

	if (state != state_loc)
		return GIT_ERROR;

	if (path != path_buffer)
		return GIT_ERROR;

	for (n = d->names; n->name; n++) {
		if (!strcmp(n->name, path)) {
			n->count++;
			return 0;
		}
	}

	return GIT_ERROR;
}


static name_data dot_names[] = {
	{ 0, "./a" },
	{ 0, "./asdf" },
	{ 0, "./pack-foo.pack" },
	{ 0, NULL }
};
static walk_data dot = {
	".",
	dot_names
};

BEGIN_TEST(dirent0, "make sure that the '.' folder is not traversed")

	must_pass(setup(&dot));

	must_pass(git_futils_direach(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &dot));

	must_pass(check_counts(&dot));

	must_pass(knockdown(&dot));
END_TEST

static name_data sub_names[] = {
	{ 0, "sub/a" },
	{ 0, "sub/asdf" },
	{ 0, "sub/pack-foo.pack" },
	{ 0, NULL }
};
static walk_data sub = {
	"sub",
	sub_names
};

BEGIN_TEST(dirent1, "traverse a subfolder")

	must_pass(setup(&sub));

	must_pass(git_futils_direach(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &sub));

	must_pass(check_counts(&sub));

	must_pass(knockdown(&sub));
END_TEST

static walk_data sub_slash = {
	"sub/",
	sub_names
};

BEGIN_TEST(dirent2, "traverse a slash-terminated subfolder")

	must_pass(setup(&sub_slash));

	must_pass(git_futils_direach(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &sub_slash));

	must_pass(check_counts(&sub_slash));

	must_pass(knockdown(&sub_slash));
END_TEST

static name_data empty_names[] = {
	{ 0, NULL }
};
static walk_data empty = {
	"empty",
	empty_names
};

static int dont_call_me(void *GIT_UNUSED(state), char *GIT_UNUSED(path))
{
	GIT_UNUSED_ARG(state)
	GIT_UNUSED_ARG(path)
	return GIT_ERROR;
}

BEGIN_TEST(dirent3, "make sure that empty folders are not traversed")

	must_pass(setup(&empty));

	must_pass(git_futils_direach(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &empty));

	must_pass(check_counts(&empty));

	/* make sure callback not called */
	must_pass(git_futils_direach(path_buffer,
			       sizeof(path_buffer),
			       dont_call_me,
			       &empty));

	must_pass(knockdown(&empty));
END_TEST

static name_data odd_names[] = {
	{ 0, "odd/.a" },
	{ 0, "odd/..c" },
	/* the following don't work on cygwin/win32 */
	/* { 0, "odd/.b." }, */
	/* { 0, "odd/..d.." },  */
	{ 0, NULL }
};
static walk_data odd = {
	"odd",
	odd_names
};

BEGIN_TEST(dirent4, "make sure that strange looking filenames ('..c') are traversed")

	must_pass(setup(&odd));

	must_pass(git_futils_direach(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &odd));

	must_pass(check_counts(&odd));

	must_pass(knockdown(&odd));
END_TEST

BEGIN_TEST(filebuf0, "make sure git_filebuf_open doesn't delete an existing lock")
	git_filebuf file;
	int fd;
	char test[] = "test", testlock[] = "test.lock";

	fd = p_creat(testlock, 0744);
	must_pass(fd);
	must_pass(p_close(fd));
	must_fail(git_filebuf_open(&file, test, 0));
	must_pass(git_futils_exists(testlock));
	must_pass(p_unlink(testlock));
END_TEST

BEGIN_TEST(filebuf1, "make sure GIT_FILEBUF_APPEND works as expected")
	git_filebuf file;
	int fd;
	char test[] = "test";

	fd = p_creat(test, 0666);
	must_pass(fd);
	must_pass(p_write(fd, "libgit2 rocks\n", 14));
	must_pass(p_close(fd));

	must_pass(git_filebuf_open(&file, test, GIT_FILEBUF_APPEND));
	must_pass(git_filebuf_printf(&file, "%s\n", "libgit2 rocks"));
	must_pass(git_filebuf_commit(&file, 0666));

	must_pass(p_unlink(test));
END_TEST

BEGIN_TEST(filebuf2, "make sure git_filebuf_write writes large buffer correctly")
	git_filebuf file;
	char test[] = "test";
	unsigned char buf[4096 * 4]; /* 2 * WRITE_BUFFER_SIZE */

	memset(buf, 0xfe, sizeof(buf));
	must_pass(git_filebuf_open(&file, test, 0));
	must_pass(git_filebuf_write(&file, buf, sizeof(buf)));
	must_pass(git_filebuf_commit(&file, 0666));

	must_pass(p_unlink(test));
END_TEST

static char *empty_tmp_dir = "test_gitfo_rmdir_recurs_test";

static int setup_empty_tmp_dir(void)
{
	char path[GIT_PATH_MAX];

	if (p_mkdir(empty_tmp_dir, 0777))
		return -1;

	git_path_join(path, empty_tmp_dir, "/one");
	if (p_mkdir(path, 0777))
		return -1;

	git_path_join(path, empty_tmp_dir, "/one/two_one");
	if (p_mkdir(path, 0777))
		return -1;

	git_path_join(path, empty_tmp_dir, "/one/two_two");
	if (p_mkdir(path, 0777))
		return -1;

	git_path_join(path, empty_tmp_dir, "/one/two_two/three");
	if (p_mkdir(path, 0777))
		return -1;

	git_path_join(path, empty_tmp_dir, "/two");
	if (p_mkdir(path, 0777))
		return -1;

	return 0;
}

BEGIN_TEST(rmdir0, "make sure empty dir can be deleted recusively")
	must_pass(setup_empty_tmp_dir());
	must_pass(git_futils_rmdir_r(empty_tmp_dir, 0));
END_TEST

BEGIN_TEST(rmdir1, "make sure non-empty dir cannot be deleted recusively")
	char file[GIT_PATH_MAX];
	int fd;

	must_pass(setup_empty_tmp_dir());
	git_path_join(file, empty_tmp_dir, "/two/file.txt");
	fd = p_creat(file, 0777);
	must_pass(fd);
	must_pass(p_close(fd));
	must_fail(git_futils_rmdir_r(empty_tmp_dir, 0));
	must_pass(p_unlink(file));
	must_pass(git_futils_rmdir_r(empty_tmp_dir, 0));
END_TEST

BEGIN_TEST(strtol0, "parsing out 32 integers from a string")
	int32_t i;

	must_pass(git__strtol32(&i, "123", NULL, 10));
	must_be_true(i == 123);

	must_pass(git__strtol32(&i, "  +123 ", NULL, 10));
	must_be_true(i == 123);

	must_pass(git__strtol32(&i, "  +2147483647 ", NULL, 10));
	must_be_true(i == 2147483647);

	must_pass(git__strtol32(&i, "  -2147483648 ", NULL, 10));
	must_be_true(i == -2147483648LL);

	must_fail(git__strtol32(&i, "  2147483657 ", NULL, 10));
	must_fail(git__strtol32(&i, "  -2147483657 ", NULL, 10));
END_TEST

BEGIN_TEST(strtol1, "parsing out 64 integers from a string")
	int64_t i;

	must_pass(git__strtol64(&i, "123", NULL, 10));
	must_be_true(i == 123);

	must_pass(git__strtol64(&i, "  +123 ", NULL, 10));
	must_be_true(i == 123);

	must_pass(git__strtol64(&i, "  +2147483647 ", NULL, 10));
	must_be_true(i == 2147483647);

	must_pass(git__strtol64(&i, "  -2147483648 ", NULL, 10));
	must_be_true(i == -2147483648LL);

	must_pass(git__strtol64(&i, "  2147483657 ", NULL, 10));
	must_be_true(i == 2147483657LL);

	must_pass(git__strtol64(&i, "  -2147483657 ", NULL, 10));
	must_be_true(i == -2147483657LL);
END_TEST

BEGIN_SUITE(core)
	ADD_TEST(string0);
	ADD_TEST(string1);

	ADD_TEST(vector0);
	ADD_TEST(vector1);
	ADD_TEST(vector2);

	ADD_TEST(path0);
	ADD_TEST(path1);
	ADD_TEST(path2);
	ADD_TEST(path5);
	ADD_TEST(path6);

	ADD_TEST(dirent0);
	ADD_TEST(dirent1);
	ADD_TEST(dirent2);
	ADD_TEST(dirent3);
	ADD_TEST(dirent4);

	ADD_TEST(filebuf0);
	ADD_TEST(filebuf1);
	ADD_TEST(filebuf2);

	ADD_TEST(rmdir0);
	ADD_TEST(rmdir1);

	ADD_TEST(strtol0);
	ADD_TEST(strtol1);
END_SUITE
