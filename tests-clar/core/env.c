#include "clar_libgit2.h"
#include "fileops.h"
#include "path.h"

#ifdef GIT_WIN32
#define NUM_VARS 5
static const char *env_vars[NUM_VARS] = {
	"HOME", "HOMEDRIVE", "HOMEPATH", "USERPROFILE", "PROGRAMFILES"
};
#else
#define NUM_VARS 1
static const char *env_vars[NUM_VARS] = { "HOME" };
#endif

static char *env_save[NUM_VARS];

void test_core_env__initialize(void)
{
	int i;
	for (i = 0; i < NUM_VARS; ++i)
		env_save[i] = cl_getenv(env_vars[i]);
}

void test_core_env__cleanup(void)
{
	int i;
	for (i = 0; i < NUM_VARS; ++i) {
		cl_setenv(env_vars[i], env_save[i]);
#ifdef GIT_WIN32
		git__free(env_save[i]);
#endif
		env_save[i] = NULL;
	}
}

static void setenv_and_check(const char *name, const char *value)
{
	char *check;
	cl_git_pass(cl_setenv(name, value));
	check = cl_getenv(name);
	cl_assert_equal_s(value, check);
#ifdef GIT_WIN32
	git__free(check);
#endif
}

void test_core_env__0(void)
{
	static char *home_values[] = {
		"fake_home",
		"fáke_hõme", /* all in latin-1 supplement */
		"fĀke_Ĥome", /* latin extended */
		"fακε_hοmέ",  /* having fun with greek */
		"faงe_นome", /* now I have no idea, but thai characters */
		"f\xe1\x9cx80ke_\xe1\x9c\x91ome", /* tagalog characters */
		"\xe1\xb8\x9fẢke_hoṁe", /* latin extended additional */
		"\xf0\x9f\x98\x98\xf0\x9f\x98\x82", /* emoticons */
		NULL
	};
	git_buf path = GIT_BUF_INIT, found = GIT_BUF_INIT;
	char testfile[16], tidx = '0';
	char **val;

	memset(testfile, 0, sizeof(testfile));
	memcpy(testfile, "testfile", 8);
	cl_assert_equal_s("testfile", testfile);

	for (val = home_values; *val != NULL; val++) {

		/* if we can't make the directory, let's just assume
		 * we are on a filesystem that doesn't support the
		 * characters in question and skip this test...
		 */
		if (p_mkdir(*val, 0777) != 0)
			continue;

		cl_git_pass(git_path_prettify(&path, *val, NULL));

		/* vary testfile name in each directory so accidentally leaving
		 * an environment variable set from a previous iteration won't
		 * accidentally make this test pass...
		 */
		testfile[8] = tidx++;
		cl_git_pass(git_buf_joinpath(&path, path.ptr, testfile));
		cl_git_mkfile(path.ptr, "find me");
		git_buf_rtruncate_at_char(&path, '/');

		cl_assert_equal_i(
			GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

		setenv_and_check("HOME", path.ptr);
		cl_git_pass(git_futils_find_global_file(&found, testfile));

		cl_setenv("HOME", env_save[0]);
		cl_assert_equal_i(
			GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

#ifdef GIT_WIN32
		setenv_and_check("HOMEDRIVE", NULL);
		setenv_and_check("HOMEPATH", NULL);
		setenv_and_check("USERPROFILE", path.ptr);

		cl_git_pass(git_futils_find_global_file(&found, testfile));

		{
			int root = git_path_root(path.ptr);
			char old;

			if (root >= 0) {
				setenv_and_check("USERPROFILE", NULL);

				cl_assert_equal_i(
					GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

				old = path.ptr[root];
				path.ptr[root] = '\0';
				setenv_and_check("HOMEDRIVE", path.ptr);
				path.ptr[root] = old;
				setenv_and_check("HOMEPATH", &path.ptr[root]);

				cl_git_pass(git_futils_find_global_file(&found, testfile));
			}
		}
#endif
	}

	git_buf_free(&path);
	git_buf_free(&found);
}

void test_core_env__1(void)
{
	git_buf path = GIT_BUF_INIT;

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_global_file(&path, "nonexistentfile"));

	cl_git_pass(cl_setenv("HOME", "doesnotexist"));
#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("HOMEPATH", "doesnotexist"));
	cl_git_pass(cl_setenv("USERPROFILE", "doesnotexist"));
#endif

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_global_file(&path, "nonexistentfile"));

	cl_git_pass(cl_setenv("HOME", NULL));
#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("HOMEPATH", NULL));
	cl_git_pass(cl_setenv("USERPROFILE", NULL));
#endif

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_global_file(&path, "nonexistentfile"));

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_system_file(&path, "nonexistentfile"));

#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("PROGRAMFILES", NULL));
	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_system_file(&path, "nonexistentfile"));
#endif

	git_buf_free(&path);
}
