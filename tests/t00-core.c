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

BEGIN_TEST("refcnt", init_inc2_dec2_free)
	git_refcnt p;

	gitrc_init(&p, 0);
	gitrc_inc(&p);
	gitrc_inc(&p);
	must_be_true(!gitrc_dec(&p));
	must_be_true(gitrc_dec(&p));
	gitrc_free(&p);
END_TEST

BEGIN_TEST("strutil", prefix_comparison)
	must_be_true(git__prefixcmp("", "") == 0);
	must_be_true(git__prefixcmp("a", "") == 0);
	must_be_true(git__prefixcmp("", "a") < 0);
	must_be_true(git__prefixcmp("a", "b") < 0);
	must_be_true(git__prefixcmp("b", "a") > 0);
	must_be_true(git__prefixcmp("ab", "a") == 0);
	must_be_true(git__prefixcmp("ab", "ac") < 0);
	must_be_true(git__prefixcmp("ab", "aa") > 0);
END_TEST

BEGIN_TEST("strutil", suffix_comparison)
	must_be_true(git__suffixcmp("", "") == 0);
	must_be_true(git__suffixcmp("a", "") == 0);
	must_be_true(git__suffixcmp("", "a") < 0);
	must_be_true(git__suffixcmp("a", "b") < 0);
	must_be_true(git__suffixcmp("b", "a") > 0);
	must_be_true(git__suffixcmp("ba", "a") == 0);
	must_be_true(git__suffixcmp("zaa", "ac") < 0);
	must_be_true(git__suffixcmp("zaz", "ac") > 0);
END_TEST

BEGIN_TEST("strutil", dirname)
	char dir[64], *dir2;

#define DIRNAME_TEST(A, B) { \
	must_be_true(git__dirname_r(dir, sizeof(dir), A) >= 0); \
	must_be_true(strcmp(dir, B) == 0);				\
	must_be_true((dir2 = git__dirname(A)) != NULL);	\
	must_be_true(strcmp(dir2, B) == 0);				\
	free(dir2);										\
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

BEGIN_TEST("strutil", basename)
	char base[64], *base2;

#define BASENAME_TEST(A, B) { \
	must_be_true(git__basename_r(base, sizeof(base), A) >= 0); \
	must_be_true(strcmp(base, B) == 0);					\
	must_be_true((base2 = git__basename(A)) != NULL);	\
	must_be_true(strcmp(base2, B) == 0);				\
	free(base2);										\
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

BEGIN_TEST("strutil", topdir)
	const char *dir;

#define TOPDIR_TEST(A, B) { \
	must_be_true((dir = git__topdir(A)) != NULL);	\
	must_be_true(strcmp(dir, B) == 0);				\
}

	TOPDIR_TEST(".git/", ".git/");
	TOPDIR_TEST("/.git/", ".git/");
	TOPDIR_TEST("usr/local/.git/", ".git/");
	TOPDIR_TEST("./.git/", ".git/");
	TOPDIR_TEST("/usr/.git/", ".git/");
	TOPDIR_TEST("/", "/");
	TOPDIR_TEST("a/", "a/");

	must_be_true(git__topdir("/usr/.git") == NULL);
	must_be_true(git__topdir(".") == NULL);
	must_be_true(git__topdir("") == NULL);
	must_be_true(git__topdir("a") == NULL);

#undef TOPDIR_TEST
END_TEST

/* Initial size of 1 will cause writing past array bounds prior to fix */
BEGIN_TEST("vector", initial_size_one)
  git_vector x;
  int i;
  git_vector_init(&x, 1, NULL, NULL);
  for (i = 0; i < 10; ++i) {
    git_vector_insert(&x, (void*) 0xabc);
  }
  git_vector_free(&x);
END_TEST

/* vector used to read past array bounds on remove() */
BEGIN_TEST("vector", remove)
  git_vector x;
  // make initial capacity exact for our insertions.
  git_vector_init(&x, 3, NULL, NULL);
  git_vector_insert(&x, (void*) 0xabc);
  git_vector_insert(&x, (void*) 0xdef);
  git_vector_insert(&x, (void*) 0x123);

  git_vector_remove(&x, 0);  // used to read past array bounds.
  git_vector_free(&x);
END_TEST



typedef int (normalize_path)(char *, const char *);

static int ensure_normalized(const char *input_path, const char *expected_path, normalize_path normalizer)
{
	int error = GIT_SUCCESS;
	char buffer_out[GIT_PATH_MAX];

	error = normalizer(buffer_out, input_path);
	if (error < GIT_SUCCESS)
		return error;

	if (expected_path == NULL)
		return error;

	if (strcmp(buffer_out, expected_path))
		error = GIT_ERROR;

	return error;
}

static int ensure_dir_path_normalized(const char *input_path, const char *expected_path)
{
	return ensure_normalized(input_path, expected_path, gitfo_prettify_dir_path);
}

static int ensure_file_path_normalized(const char *input_path, const char *expected_path)
{
	return ensure_normalized(input_path, expected_path, gitfo_prettify_file_path);
}

BEGIN_TEST("path", file_path_prettifying)
	must_pass(ensure_file_path_normalized("a", "a"));
	must_pass(ensure_file_path_normalized("./testrepo.git", "testrepo.git"));
	must_pass(ensure_file_path_normalized("./.git", ".git"));
	must_pass(ensure_file_path_normalized("./git.", "git."));
	must_fail(ensure_file_path_normalized("git./", NULL));
	must_fail(ensure_file_path_normalized("", NULL));
	must_fail(ensure_file_path_normalized(".", NULL));
	must_fail(ensure_file_path_normalized("./", NULL));
	must_fail(ensure_file_path_normalized("./.", NULL));
	must_fail(ensure_file_path_normalized("./..", NULL));
	must_fail(ensure_file_path_normalized("../.", NULL));
	must_fail(ensure_file_path_normalized("./.././/", NULL));
	must_fail(ensure_file_path_normalized("dir/..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/..///..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub///../..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub///..///..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../../..", NULL));
	must_pass(ensure_file_path_normalized("dir", "dir"));
	must_fail(ensure_file_path_normalized("dir//", NULL));
	must_pass(ensure_file_path_normalized("./dir", "dir"));
	must_fail(ensure_file_path_normalized("dir/.", NULL));
	must_fail(ensure_file_path_normalized("dir///./", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/..", NULL));
	must_fail(ensure_file_path_normalized("dir//sub/..",NULL));
	must_fail(ensure_file_path_normalized("dir//sub/../", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../.", NULL));
	must_fail(ensure_file_path_normalized("dir/s1/../s2/", NULL));
	must_fail(ensure_file_path_normalized("d1/s1///s2/..//../s3/", NULL));
	must_pass(ensure_file_path_normalized("d1/s1//../s2/../../d2", "d2"));
	must_fail(ensure_file_path_normalized("dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("....", NULL));
	must_fail(ensure_file_path_normalized("...", NULL));
	must_fail(ensure_file_path_normalized("./...", NULL));
	must_fail(ensure_file_path_normalized("d1/...", NULL));
	must_fail(ensure_file_path_normalized("d1/.../", NULL));
	must_fail(ensure_file_path_normalized("d1/.../d2", NULL));
	
	must_pass(ensure_file_path_normalized("/a", "/a"));
	must_pass(ensure_file_path_normalized("/./testrepo.git", "/testrepo.git"));
	must_pass(ensure_file_path_normalized("/./.git", "/.git"));
	must_pass(ensure_file_path_normalized("/./git.", "/git."));
	must_fail(ensure_file_path_normalized("/git./", NULL));
	must_fail(ensure_file_path_normalized("/", NULL));
	must_fail(ensure_file_path_normalized("/.", NULL));
	must_fail(ensure_file_path_normalized("/./", NULL));
	must_fail(ensure_file_path_normalized("/./.", NULL));
	must_fail(ensure_file_path_normalized("/./..", NULL));
	must_fail(ensure_file_path_normalized("/../.", NULL));
	must_fail(ensure_file_path_normalized("/./.././/", NULL));
	must_fail(ensure_file_path_normalized("/dir/..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/..///..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub///../..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub///..///..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../../..", NULL));
	must_pass(ensure_file_path_normalized("/dir", "/dir"));
	must_fail(ensure_file_path_normalized("/dir//", NULL));
	must_pass(ensure_file_path_normalized("/./dir", "/dir"));
	must_fail(ensure_file_path_normalized("/dir/.", NULL));
	must_fail(ensure_file_path_normalized("/dir///./", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/..", NULL));
	must_fail(ensure_file_path_normalized("/dir//sub/..",NULL));
	must_fail(ensure_file_path_normalized("/dir//sub/../", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../.", NULL));
	must_fail(ensure_file_path_normalized("/dir/s1/../s2/", NULL));
	must_fail(ensure_file_path_normalized("/d1/s1///s2/..//../s3/", NULL));
	must_pass(ensure_file_path_normalized("/d1/s1//../s2/../../d2", "/d2"));
	must_fail(ensure_file_path_normalized("/dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("/....", NULL));
	must_fail(ensure_file_path_normalized("/...", NULL));
	must_fail(ensure_file_path_normalized("/./...", NULL));
	must_fail(ensure_file_path_normalized("/d1/...", NULL));
	must_fail(ensure_file_path_normalized("/d1/.../", NULL));
	must_fail(ensure_file_path_normalized("/d1/.../d2", NULL));
END_TEST

BEGIN_TEST("path", dir_path_prettifying)
	must_pass(ensure_dir_path_normalized("./testrepo.git", "testrepo.git/"));
	must_pass(ensure_dir_path_normalized("./.git", ".git/"));
	must_pass(ensure_dir_path_normalized("./git.", "git./"));
	must_pass(ensure_dir_path_normalized("git./", "git./"));
	must_pass(ensure_dir_path_normalized("", ""));
	must_pass(ensure_dir_path_normalized(".", ""));
	must_pass(ensure_dir_path_normalized("./", ""));
	must_pass(ensure_dir_path_normalized("./.", ""));
	must_fail(ensure_dir_path_normalized("./..", NULL));
	must_fail(ensure_dir_path_normalized("../.", NULL));
	must_fail(ensure_dir_path_normalized("./.././/", NULL));
	must_pass(ensure_dir_path_normalized("dir/..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub/../..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub/..///..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub///../..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub///..///..", ""));
	must_fail(ensure_dir_path_normalized("dir/sub/../../..", NULL));
	must_pass(ensure_dir_path_normalized("dir", "dir/"));
	must_pass(ensure_dir_path_normalized("dir//", "dir/"));
	must_pass(ensure_dir_path_normalized("./dir", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/.", "dir/"));
	must_pass(ensure_dir_path_normalized("dir///./", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/sub/..", "dir/"));
	must_pass(ensure_dir_path_normalized("dir//sub/..", "dir/"));
	must_pass(ensure_dir_path_normalized("dir//sub/../", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/sub/../", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/sub/../.", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/s1/../s2/", "dir/s2/"));
	must_pass(ensure_dir_path_normalized("d1/s1///s2/..//../s3/", "d1/s3/"));
	must_pass(ensure_dir_path_normalized("d1/s1//../s2/../../d2", "d2/"));
	must_pass(ensure_dir_path_normalized("dir/sub/../", "dir/"));
	must_fail(ensure_dir_path_normalized("....", NULL));
	must_fail(ensure_dir_path_normalized("...", NULL));
	must_fail(ensure_dir_path_normalized("./...", NULL));
	must_fail(ensure_dir_path_normalized("d1/...", NULL));
	must_fail(ensure_dir_path_normalized("d1/.../", NULL));
	must_fail(ensure_dir_path_normalized("d1/.../d2", NULL));

	must_pass(ensure_dir_path_normalized("/./testrepo.git", "/testrepo.git/"));
	must_pass(ensure_dir_path_normalized("/./.git", "/.git/"));
	must_pass(ensure_dir_path_normalized("/./git.", "/git./"));
	must_pass(ensure_dir_path_normalized("/git./", "/git./"));
	must_pass(ensure_dir_path_normalized("/", "/"));
	must_pass(ensure_dir_path_normalized("//", "/"));
	must_pass(ensure_dir_path_normalized("///", "/"));
	must_pass(ensure_dir_path_normalized("/.", "/"));
	must_pass(ensure_dir_path_normalized("/./", "/"));
	must_fail(ensure_dir_path_normalized("/./..", NULL));
	must_fail(ensure_dir_path_normalized("/../.", NULL));
	must_fail(ensure_dir_path_normalized("/./.././/", NULL));
	must_pass(ensure_dir_path_normalized("/dir/..", "/"));
	must_pass(ensure_dir_path_normalized("/dir/sub/../..", "/"));
	must_fail(ensure_dir_path_normalized("/dir/sub/../../..", NULL));
	must_pass(ensure_dir_path_normalized("/dir", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir//", "/dir/"));
	must_pass(ensure_dir_path_normalized("/./dir", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir/.", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir///./", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir//sub/..", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir/sub/../", "/dir/"));
	must_pass(ensure_dir_path_normalized("//dir/sub/../.", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir/s1/../s2/", "/dir/s2/"));
	must_pass(ensure_dir_path_normalized("/d1/s1///s2/..//../s3/", "/d1/s3/"));
	must_pass(ensure_dir_path_normalized("/d1/s1//../s2/../../d2", "/d2/"));
	must_fail(ensure_dir_path_normalized("/....", NULL));
	must_fail(ensure_dir_path_normalized("/...", NULL));
	must_fail(ensure_dir_path_normalized("/./...", NULL));
	must_fail(ensure_dir_path_normalized("/d1/...", NULL));
	must_fail(ensure_dir_path_normalized("/d1/.../", NULL));
	must_fail(ensure_dir_path_normalized("/d1/.../d2", NULL));
END_TEST

static int ensure_joinpath(const char *path_a, const char *path_b, const char *expected_path)
{
	char joined_path[GIT_PATH_MAX];
	git__joinpath(joined_path, path_a, path_b);
	return strcmp(joined_path, expected_path) == 0 ? GIT_SUCCESS : GIT_ERROR;
}

BEGIN_TEST("path", joinpath)
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
	git__joinpath_n(joined_path, 4, path_a, path_b, path_c, path_d);
	return strcmp(joined_path, expected_path) == 0 ? GIT_SUCCESS : GIT_ERROR;
}

BEGIN_TEST("path", joinpath_n)
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

	if (gitfo_mkdir(top_dir, 0755) < 0)
		return error("can't mkdir(\"%s\")", top_dir);

	if (gitfo_chdir(top_dir) < 0)
		return error("can't chdir(\"%s\")", top_dir);

	if (strcmp(d->sub, ".") != 0)
		if (gitfo_mkdir(d->sub, 0755) < 0)
			return error("can't mkdir(\"%s\")", d->sub);

	strcpy(path_buffer, d->sub);
	state_loc = d;

	for (n = d->names; n->name; n++) {
		git_file fd = gitfo_creat(n->name, 0600);
		if (fd < 0)
			return GIT_ERROR;
		gitfo_close(fd);
		n->count = 0;
	}

	return 0;
}

static int knockdown(walk_data *d)
{
	name_data *n;

	for (n = d->names; n->name; n++) {
		if (gitfo_unlink(n->name) < 0)
			return error("can't unlink(\"%s\")", n->name);
	}

	if (strcmp(d->sub, ".") != 0)
		if (gitfo_rmdir(d->sub) < 0)
			return error("can't rmdir(\"%s\")", d->sub);

	if (gitfo_chdir("..") < 0)
		return error("can't chdir(\"..\")");

	if (gitfo_rmdir(top_dir) < 0)
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

BEGIN_TEST("dirent", dot)

	must_pass(setup(&dot));

	must_pass(gitfo_dirent(path_buffer,
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

BEGIN_TEST("dirent", sub)

	must_pass(setup(&sub));

	must_pass(gitfo_dirent(path_buffer,
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

BEGIN_TEST("dirent", sub_slash)

	must_pass(setup(&sub_slash));

	must_pass(gitfo_dirent(path_buffer,
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

BEGIN_TEST("dirent", empty)

	must_pass(setup(&empty));

	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &empty));

	must_pass(check_counts(&empty));

	/* make sure callback not called */
	must_pass(gitfo_dirent(path_buffer,
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

BEGIN_TEST("dirent", odd)

	must_pass(setup(&odd));

	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &odd));

	must_pass(check_counts(&odd));

	must_pass(knockdown(&odd));
END_TEST


git_testsuite *libgit2_suite_core(void)
{
	git_testsuite *suite = git_testsuite_new("Core");

	ADD_TEST(suite, "refcnt", init_inc2_dec2_free);

	ADD_TEST(suite, "strutil", prefix_comparison);
	ADD_TEST(suite, "strutil", suffix_comparison);
	ADD_TEST(suite, "strutil", dirname);
	ADD_TEST(suite, "strutil", basename);
	ADD_TEST(suite, "strutil", topdir);

	ADD_TEST(suite, "vector", initial_size_one);
	ADD_TEST(suite, "vector", remove);

	ADD_TEST(suite, "path", file_path_prettifying);
	ADD_TEST(suite, "path", dir_path_prettifying);
	ADD_TEST(suite, "path", joinpath);
	ADD_TEST(suite, "path", joinpath_n);

	ADD_TEST(suite, "dirent", dot);
	ADD_TEST(suite, "dirent", sub);
	ADD_TEST(suite, "dirent", sub_slash);
	ADD_TEST(suite, "dirent", empty);
	ADD_TEST(suite, "dirent", odd);

	return suite;
}
