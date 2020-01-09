#include "clar_libgit2.h"

#include "git2/clone.h"
#include "clone.h"
#include "buffer.h"
#include "futils.h"

#ifdef GIT_WIN32
void ensure_path_exists(git_buf *path)
{
	git_win32_path wpath;
	git_win32_path current_dir;
	wchar_t *end;
	ZeroMemory(current_dir, sizeof(current_dir));

	git_win32_path_from_utf8(wpath, path->ptr);
	end = wcschr(wpath, L':');
	end = wcschr(++end, L'\\');
	end = wcschr(++end, L'\\');

	while (end != NULL) {
		wcsncpy(current_dir, wpath, end - wpath + 1);
		if (!CreateDirectoryW(current_dir, NULL)) {
			cl_assert(GetLastError() == ERROR_ALREADY_EXISTS);
		}
		end = wcschr(++end, L'\\');
	}
}

void generate_long_path(git_buf *path, size_t len)
{
	const char *base = clar_sandbox_path();
	size_t base_len = strlen(base);
	size_t remain = len - base_len;
	size_t i;

	git_buf_clear(path);
	git_buf_puts(path, base);
	git_buf_putc(path, '/');

	cl_assert(remain < (len - 5));

	for (i = 0; i < (remain - 5); i++) {
		/* add a slash every 240 characters, but not as first or last character */
		if (i % 239 == 0 && i != remain - 6 && i != 0) {
			git_buf_putc(path, '/');
		} else {
			git_buf_putc(path, 'a');
		}
	}
	ensure_path_exists(path);
}

void assert_name_too_long(void)
{
	const git_error *err;
	size_t expected_len, actual_len;
	char *expected_msg;

	err = git_error_last();
	actual_len = strlen(err->message);

	expected_msg = git_win32_get_error_message(ERROR_FILENAME_EXCED_RANGE);
	expected_len = strlen(expected_msg);

	/* check the suffix */
	cl_assert_equal_s(expected_msg, err->message + (actual_len - expected_len));

	git__free(expected_msg);
}
#endif

void test_win32_longpath__errmsg_on_checkout(void)
{
#ifdef GIT_WIN32
	git_repository *repo;
	git_buf path = GIT_BUF_INIT;

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_WINDOWS_LONGPATHS, 0));
	generate_long_path(&path, WIN_GIT_SHORT_PATH_MAX);
	cl_git_fail(git_clone(&repo, cl_fixture("testrepo.git"), path.ptr, NULL));
	assert_name_too_long();

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_WINDOWS_LONGPATHS, 1));
	generate_long_path(&path, WIN_GIT_PATH_MAX);
	cl_git_fail(git_clone(&repo, cl_fixture("testrepo.git"), path.ptr, NULL));
	assert_name_too_long();

	git_buf_dispose(&path);
#endif
}

void test_win32_longpath__longest_path(void)
{
#ifdef GIT_WIN32
	git_repository *repo;
	git_buf path = GIT_BUF_INIT;

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_WINDOWS_LONGPATHS, 1));
	/* account for length of pack file path */
	generate_long_path(&path, WIN_GIT_PATH_MAX - 66);
	cl_git_pass(git_clone(&repo, cl_fixture("testrepo.git"), path.ptr, NULL));

	git_buf_dispose(&path);
	git_repository_free(repo);
#endif
}

void test_win32_longpath__opt(void)
{
#ifdef GIT_WIN32
	int longpaths;

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_WINDOWS_LONGPATHS, 1));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_WINDOWS_LONGPATHS, &longpaths));
	cl_assert(longpaths == 1);

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_WINDOWS_LONGPATHS, 0));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_WINDOWS_LONGPATHS, &longpaths));
	cl_assert(longpaths == 0);
#endif
}
