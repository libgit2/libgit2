#include "clar_libgit2.h"
#include "common.h"
#include "util.h"
#include "posix.h"

#ifdef git__throw
void test_core_errors__old_school(void)
{
	git_clearerror();
	cl_assert(git_lasterror() == NULL);

	cl_assert(git_strerror(GIT_ENOTFOUND) != NULL);

	git__throw(GIT_ENOTFOUND, "My Message");
	cl_assert(git_lasterror() != NULL);
	cl_assert(git__prefixcmp(git_lasterror(), "My Message") == 0);
	git_clearerror();
}
#endif

#ifdef GITERR_CHECK_ALLOC
void test_core_errors__new_school(void)
{
	char *str_in_error;

	git_error_clear();
	cl_assert(git_error_last() == NULL);

	giterr_set_oom(); /* internal fn */

	cl_assert(git_error_last() != NULL);
	cl_assert(git_error_last()->klass == GITERR_NOMEMORY);
	str_in_error = strstr(git_error_last()->message, "memory");
	cl_assert(str_in_error != NULL);

	git_error_clear();

	giterr_set(GITERR_REPOSITORY, "This is a test"); /* internal fn */

	cl_assert(git_error_last() != NULL);
	str_in_error = strstr(git_error_last()->message, "This is a test");
	cl_assert(str_in_error != NULL);

	git_error_clear();

	{
		struct stat st;
		assert(p_lstat("this_file_does_not_exist", &st) < 0);
		GIT_UNUSED(st);
	}
	giterr_set(GITERR_OS, "stat failed"); /* internal fn */

	cl_assert(git_error_last() != NULL);
	str_in_error = strstr(git_error_last()->message, "stat failed");
	cl_assert(str_in_error != NULL);
	cl_assert(git__prefixcmp(str_in_error, "stat failed: ") == 0);
	cl_assert(strlen(str_in_error) > strlen("stat failed: "));

#ifdef GIT_WIN32
	git_error_clear();

	/* The MSDN docs use this to generate a sample error */
	cl_assert(GetProcessId(NULL) == 0);
	giterr_set(GITERR_OS, "GetProcessId failed"); /* internal fn */

	cl_assert(git_error_last() != NULL);
	str_in_error = strstr(git_error_last()->message, "GetProcessId failed");
	cl_assert(str_in_error != NULL);
	cl_assert(git__prefixcmp(str_in_error, "GetProcessId failed: ") == 0);
	cl_assert(strlen(str_in_error) > strlen("GetProcessId failed: "));
#endif

	git_error_clear();
}
#endif
