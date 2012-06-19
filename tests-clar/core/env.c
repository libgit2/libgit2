#include "clar_libgit2.h"
#include "fileops.h"
#include "path.h"

#ifdef GIT_WIN32
static char *env_userprofile = NULL;
static char *env_programfiles = NULL;
#else
static char *env_home = NULL;
#endif

void test_core_env__initialize(void)
{
#ifdef GIT_WIN32
	env_userprofile = cl_getenv("USERPROFILE");
	env_programfiles = cl_getenv("PROGRAMFILES");
#else
	env_home = cl_getenv("HOME");
#endif
}

void test_core_env__cleanup(void)
{
#ifdef GIT_WIN32
	cl_setenv("USERPROFILE", env_userprofile);
	git__free(env_userprofile);
	cl_setenv("PROGRAMFILES", env_programfiles);
	git__free(env_programfiles);
#else
	cl_setenv("HOME", env_home);
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
	char **val;
	char *check;

	for (val = home_values; *val != NULL; val++) {

		if (p_mkdir(*val, 0777) == 0) {
			/* if we can't make the directory, let's just assume
			 * we are on a filesystem that doesn't support the
			 * characters in question and skip this test...
			 */
			cl_git_pass(git_path_prettify(&path, *val, NULL));

#ifdef GIT_WIN32
			cl_git_pass(cl_setenv("USERPROFILE", path.ptr));

			/* do a quick check that it was set correctly */
			check = cl_getenv("USERPROFILE");
			cl_assert_equal_s(path.ptr, check);
			git__free(check);
#else
			cl_git_pass(cl_setenv("HOME", path.ptr));

			/* do a quick check that it was set correctly */
			check = cl_getenv("HOME");
			cl_assert_equal_s(path.ptr, check);
#endif

			cl_git_pass(git_buf_puts(&path, "/testfile"));
			cl_git_mkfile(path.ptr, "find me");

			cl_git_pass(git_futils_find_global_file(&found, "testfile"));
		}
	}

	git_buf_free(&path);
	git_buf_free(&found);
}

void test_core_env__1(void)
{
	git_buf path = GIT_BUF_INIT;

	cl_assert(git_futils_find_global_file(&path, "nonexistentfile") == GIT_ENOTFOUND);

#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("USERPROFILE", "doesnotexist"));
#else
	cl_git_pass(cl_setenv("HOME", "doesnotexist"));
#endif

	cl_assert(git_futils_find_global_file(&path, "nonexistentfile") == GIT_ENOTFOUND);

#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("USERPROFILE", NULL));
#else
	cl_git_pass(cl_setenv("HOME", NULL));
#endif

	cl_assert(git_futils_find_global_file(&path, "nonexistentfile") == -1);

	cl_assert(git_futils_find_system_file(&path, "nonexistentfile") == GIT_ENOTFOUND);

#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("PROGRAMFILES", NULL));

	cl_assert(git_futils_find_system_file(&path, "nonexistentfile") == -1);
#endif

	git_buf_free(&path);
}
