#include "clar_libgit2.h"

#include "git2/clone.h"
#include "clone.h"
#include "buffer.h"
#include "futils.h"
#include "repository.h"

static git_buf path = GIT_BUF_INIT;

void test_win32_longpath__initialize(void)
{
#ifdef GIT_WIN32
	const char *base = clar_sandbox_path();
	size_t base_len = strlen(base);
	size_t remain = MAX_PATH - base_len;
	size_t i;

	git_buf_clear(&path);
	git_buf_puts(&path, base);
	git_buf_putc(&path, '/');

	cl_assert(remain < (MAX_PATH - 5));

	for (i = 0; i < (remain - 5); i++)
		git_buf_putc(&path, 'a');
#endif
}

void test_win32_longpath__cleanup(void)
{
	git_buf_dispose(&path);
	cl_git_sandbox_cleanup();
}

void test_win32_longpath__errmsg_on_checkout(void)
{
#ifdef GIT_WIN32
	git_repository *repo;

	cl_git_fail(git_clone(&repo, cl_fixture("testrepo.git"), path.ptr, NULL));
	cl_assert(git__prefixcmp(git_error_last()->message, "path too long") == 0);
#endif
}

void test_win32_longpath__workdir_path_validated(void)
{
#ifdef GIT_WIN32
	git_repository *repo = cl_git_sandbox_init("testrepo");
	git_buf out = GIT_BUF_INIT;

	cl_git_pass(git_repository_workdir_path(&out, repo, "a.txt"));

	/* even if the repo path is a drive letter, this is too long */
	cl_git_fail(git_repository_workdir_path(&out, repo, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt"));
	cl_assert(git__prefixcmp(git_error_last()->message, "path too long") == 0);

	cl_repo_set_bool(repo, "core.longpaths", true);
	cl_git_pass(git_repository_workdir_path(&out, repo, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt"));
	git_buf_dispose(&out);
#endif
}
