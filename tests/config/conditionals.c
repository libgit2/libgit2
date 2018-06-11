#include "clar_libgit2.h"
#include "buffer.h"
#include "fileops.h"

#ifdef GIT_WIN32
# define ROOT_PREFIX "C:"
#else
# define ROOT_PREFIX
#endif

static git_repository *_repo;

void test_config_conditionals__initialize(void)
{
	_repo = cl_git_sandbox_init("empty_standard_repo");
}

void test_config_conditionals__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void assert_condition_includes(const char *keyword, const char *path, bool expected)
{
	git_config *cfg;
	git_buf buf = GIT_BUF_INIT;

	git_buf_printf(&buf, "[includeIf \"%s:%s\"]\n", keyword, path);
	git_buf_puts(&buf, "path = other\n");

	cl_git_mkfile("empty_standard_repo/.git/config", buf.ptr);
	cl_git_mkfile("empty_standard_repo/.git/other", "[foo]\nbar=baz\n");
	_repo = cl_git_sandbox_reopen();

	cl_git_pass(git_repository_config(&cfg, _repo));

	if (expected) {
		git_buf_clear(&buf);
		cl_git_pass(git_config_get_string_buf(&buf, cfg, "foo.bar"));
		cl_assert_equal_s("baz", git_buf_cstr(&buf));
	} else {
		cl_git_fail_with(GIT_ENOTFOUND,
				 git_config_get_string_buf(&buf, cfg, "foo.bar"));
	}

	git_buf_dispose(&buf);
	git_config_free(cfg);
}

void test_config_conditionals__gitdir(void)
{
	git_buf path = GIT_BUF_INIT;
	char *sandbox_path;

	assert_condition_includes("gitdir", ROOT_PREFIX "/", true);
	assert_condition_includes("gitdir", "empty_standard_repo", true);
	assert_condition_includes("gitdir", "empty_standard_repo/", true);
	assert_condition_includes("gitdir", "./", true);

	assert_condition_includes("gitdir", ROOT_PREFIX "/nonexistent", false);
	assert_condition_includes("gitdir", ROOT_PREFIX "/empty_standard_repo", false);
	assert_condition_includes("gitdir", "empty_stand", false);
	assert_condition_includes("gitdir", "~/empty_standard_repo", false);

	sandbox_path = p_realpath(clar_sandbox_path(), NULL);

	git_buf_joinpath(&path, sandbox_path, "/");
	assert_condition_includes("gitdir", path.ptr, true);

	git_buf_joinpath(&path, sandbox_path, "/*");
	assert_condition_includes("gitdir", path.ptr, true);

	git_buf_joinpath(&path, sandbox_path, "empty_standard_repo");
	assert_condition_includes("gitdir", path.ptr, true);

	git_buf_joinpath(&path, sandbox_path, "Empty_Standard_Repo");
	assert_condition_includes("gitdir", path.ptr, false);

	git__free(sandbox_path);
	git_buf_dispose(&path);
}

void test_config_conditionals__gitdir_i(void)
{
	git_buf path = GIT_BUF_INIT;
	char *sandbox_path;

	sandbox_path = p_realpath(clar_sandbox_path(), NULL);

	git_buf_joinpath(&path, sandbox_path, "empty_standard_repo");
	assert_condition_includes("gitdir/i", path.ptr, true);

	git_buf_joinpath(&path, sandbox_path, "EMPTY_STANDARD_REPO");
	assert_condition_includes("gitdir/i", path.ptr, true);

	git__free(sandbox_path);
	git_buf_dispose(&path);
}

void test_config_conditionals__invalid_conditional_fails(void)
{
	assert_condition_includes("foobar", ".git", false);
}
