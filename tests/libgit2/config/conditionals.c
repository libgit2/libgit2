#include "clar_libgit2.h"
#include "futils.h"
#include "repository.h"

#ifdef GIT_WIN32
# define ROOT_PREFIX "C:"
#else
# define ROOT_PREFIX
#endif

static git_repository *_repo;
static char *_old_pwd;

void test_config_conditionals__initialize(void)
{
	_repo = cl_git_sandbox_init("empty_standard_repo");
	_old_pwd = cl_getenv("PWD");
}

void test_config_conditionals__cleanup(void)
{
	cl_must_pass(p_chdir(clar_sandbox_path()));
	cl_git_pass(cl_setenv("PWD", _old_pwd));
	git__free(_old_pwd);
	_old_pwd = NULL;
	p_unlink("conditional-link");
	cl_git_sandbox_cleanup();
}

static void assert_condition_includes(const char *keyword, const char *path, bool expected)
{
	git_buf value = GIT_BUF_INIT;
	git_str key = GIT_STR_INIT;
	git_str buf = GIT_STR_INIT;
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, _repo));
	cl_git_pass(git_str_printf(&key, "includeIf.%s:%s.path", keyword, path));
	cl_git_pass(git_config_set_string(cfg, key.ptr, "other"));
	git_config_free(cfg);

	cl_git_mkfile("empty_standard_repo/.git/other", "[foo]\nbar=baz\n");
	_repo = cl_git_sandbox_reopen();

	git_str_dispose(&buf);

	cl_git_pass(git_repository_config(&cfg, _repo));

	if (expected) {
		cl_git_pass(git_config_get_string_buf(&value, cfg, "foo.bar"));
		cl_assert_equal_s("baz", value.ptr);
	} else {
		cl_git_fail_with(GIT_ENOTFOUND,
				 git_config_get_string_buf(&value, cfg, "foo.bar"));
	}

	cl_git_pass(git_config_delete_entry(cfg, key.ptr));

	git_str_dispose(&key);
	git_str_dispose(&buf);
	git_buf_dispose(&value);
	git_config_free(cfg);
}

static char *sandbox_path(git_str *buf, const char *suffix)
{
	char *path = p_realpath(clar_sandbox_path(), NULL);
	cl_assert(path);
	cl_git_pass(git_str_attach(buf, path, 0));
	cl_git_pass(git_str_joinpath(buf, buf->ptr, suffix));
	return buf->ptr;
}

void test_config_conditionals__gitdir(void)
{
	git_str path = GIT_STR_INIT;

	assert_condition_includes("gitdir", ROOT_PREFIX "/", true);
	assert_condition_includes("gitdir", "empty_stand", false);
	assert_condition_includes("gitdir", "empty_stand/", false);
	assert_condition_includes("gitdir", "empty_stand/.git", false);
	assert_condition_includes("gitdir", "empty_stand/.git/", false);
	assert_condition_includes("gitdir", "empty_stand*/", true);
	assert_condition_includes("gitdir", "empty_stand*/.git", true);
	assert_condition_includes("gitdir", "empty_stand*/.git/", false);
	assert_condition_includes("gitdir", "empty_standard_repo", false);
	assert_condition_includes("gitdir", "empty_standard_repo/", true);
	assert_condition_includes("gitdir", "empty_standard_repo/.git", true);
	assert_condition_includes("gitdir", "empty_standard_repo/.git/", false);

	assert_condition_includes("gitdir", "./", false);

	assert_condition_includes("gitdir", ROOT_PREFIX "/nonexistent", false);
	assert_condition_includes("gitdir", ROOT_PREFIX "/empty_standard_repo", false);
	assert_condition_includes("gitdir", "~/empty_standard_repo", false);

	assert_condition_includes("gitdir", sandbox_path(&path, "/"), true);
	assert_condition_includes("gitdir", sandbox_path(&path, "/*"), false);
	assert_condition_includes("gitdir", sandbox_path(&path, "/**"), true);

	assert_condition_includes("gitdir", sandbox_path(&path, "empty_standard_repo"), false);
	assert_condition_includes("gitdir", sandbox_path(&path, "empty_standard_repo/"), true);
	assert_condition_includes("gitdir", sandbox_path(&path, "empty_standard_repo/"), true);
	assert_condition_includes("gitdir", sandbox_path(&path, "Empty_Standard_Repo"), false);
	assert_condition_includes("gitdir", sandbox_path(&path, "Empty_Standard_Repo/"), false);

	git_str_dispose(&path);
}

void test_config_conditionals__gitdir_i(void)
{
	git_str path = GIT_STR_INIT;

	assert_condition_includes("gitdir/i", sandbox_path(&path, "empty_standard_repo/"), true);
	assert_condition_includes("gitdir/i", sandbox_path(&path, "EMPTY_STANDARD_REPO/"), true);

	git_str_dispose(&path);
}

#ifndef GIT_WIN32
static void setup_symlink_condition(void)
{
	git_config *cfg;
	git_str path = GIT_STR_INIT, key = GIT_STR_INIT;

	cl_must_pass(symlink(".", "conditional-link"));
	cl_git_mkfile("empty_standard_repo/.git/other", "[foo]\nbar=baz\n");
	cl_git_pass(git_repository_config(&cfg, _repo));
	cl_git_pass(git_str_printf(&key, "includeIf.gitdir:%s.path",
		sandbox_path(&path, "conditional-link/")));
	cl_git_pass(git_config_set_string(cfg, key.ptr, "other"));
	git_config_free(cfg);
	git_str_dispose(&key);
	git_str_dispose(&path);
}

static void assert_repository_condition(git_repository *repo, bool expected)
{
	git_config *cfg;
	git_buf value = GIT_BUF_INIT;
	int result;

	cl_git_pass(git_repository_config(&cfg, repo));
	result = git_config_get_string_buf(&value, cfg, "foo.bar");
	if (!result)
		cl_assert_equal_s("baz", value.ptr);
	git_buf_dispose(&value);
	git_config_free(cfg);
	git_repository_free(repo);
	cl_assert_equal_i(expected ? 0 : GIT_ENOTFOUND, result);
}

static void assert_symlink_condition(const char *path, bool expected)
{
	git_repository *repo;
	cl_git_pass(git_repository_open(&repo, path));
	assert_repository_condition(repo, expected);
}
#endif

void test_config_conditionals__gitdir_symlink_path(void)
{
#ifndef GIT_WIN32
	setup_symlink_condition();
	assert_symlink_condition("conditional-link/empty_standard_repo", true);
#else
	cl_skip();
#endif
}

void test_config_conditionals__gitdir_does_not_resolve_pattern(void)
{
#ifndef GIT_WIN32
	setup_symlink_condition();
	assert_symlink_condition("empty_standard_repo", false);
#else
	cl_skip();
#endif
}

void test_config_conditionals__gitdir_symlink_pwd(void)
{
#ifndef GIT_WIN32
	git_str path = GIT_STR_INIT;

	setup_symlink_condition();
	cl_git_pass(cl_setenv("PWD",
		sandbox_path(&path, "conditional-link/empty_standard_repo")));
	git_str_dispose(&path);
	cl_must_pass(p_chdir("empty_standard_repo"));
	assert_symlink_condition(".", true);
#else
	cl_skip();
#endif
}

void test_config_conditionals__gitdir_stale_pwd(void)
{
#ifndef GIT_WIN32
	git_str path = GIT_STR_INIT;

	setup_symlink_condition();
	cl_git_pass(cl_setenv("PWD",
		sandbox_path(&path, "conditional-link/empty_standard_repo")));
	git_str_dispose(&path);
	assert_symlink_condition("empty_standard_repo", false);
#else
	cl_skip();
#endif
}

void test_config_conditionals__gitdir_absolute_path_ignores_pwd(void)
{
#ifndef GIT_WIN32
	git_str path = GIT_STR_INIT;

	setup_symlink_condition();
	cl_git_pass(cl_setenv("PWD", sandbox_path(&path, "conditional-link")));
	assert_symlink_condition(sandbox_path(&path, "empty_standard_repo"), false);
	git_str_dispose(&path);
#else
	cl_skip();
#endif
}

void test_config_conditionals__gitdir_symlink_bare(void)
{
#ifndef GIT_WIN32
	git_repository *repo;

	setup_symlink_condition();
	cl_git_pass(git_repository_open_bare(&repo,
		"conditional-link/empty_standard_repo/.git"));
	assert_repository_condition(repo, true);
#else
	cl_skip();
#endif
}

void test_config_conditionals__gitdir_symlink_discovery(void)
{
#ifndef GIT_WIN32
	git_repository *repo;

	setup_symlink_condition();
	cl_must_pass(p_mkdir("empty_standard_repo/subdir", 0777));
	cl_git_pass(git_repository_open_ext(&repo,
		"conditional-link/empty_standard_repo/subdir", 0, NULL));
	assert_repository_condition(repo, true);
#else
	cl_skip();
#endif
}

void test_config_conditionals__gitdir_gitfile_uses_resolved_target(void)
{
#ifndef GIT_WIN32
	setup_symlink_condition();
	cl_must_pass(p_mkdir("empty_standard_repo/linked", 0777));
	cl_git_mkfile("empty_standard_repo/linked/.git", "gitdir: ../.git\n");
	assert_symlink_condition("conditional-link/empty_standard_repo/linked", false);
#else
	cl_skip();
#endif
}

void test_config_conditionals__invalid_conditional_fails(void)
{
	assert_condition_includes("foobar", ".git", false);
}

static void set_head(git_repository *repo, const char *name)
{
	struct git_reference *ref;
	cl_git_pass(git_reference_symbolic_create(&ref, repo, GIT_HEAD_FILE, name, 1, NULL));
	git_reference_free(ref);
}

void test_config_conditionals__onbranch(void)
{
	assert_condition_includes("onbranch", "master", true);
	assert_condition_includes("onbranch", "m*", true);
	assert_condition_includes("onbranch", "*", true);
	assert_condition_includes("onbranch", "master/", false);
	assert_condition_includes("onbranch", "foo", false);

	set_head(_repo, "refs/heads/foo");
	assert_condition_includes("onbranch", "master", false);
	assert_condition_includes("onbranch", "foo", true);
	assert_condition_includes("onbranch", "f*o", true);

	set_head(_repo, "refs/heads/dir/ref");
	assert_condition_includes("onbranch", "dir/ref", true);
	assert_condition_includes("onbranch", "dir/", true);
	assert_condition_includes("onbranch", "dir/*", true);
	assert_condition_includes("onbranch", "dir/**", true);
	assert_condition_includes("onbranch", "**", true);
	assert_condition_includes("onbranch", "dir", false);
	assert_condition_includes("onbranch", "dir*", false);

	set_head(_repo, "refs/heads/dir/subdir/ref");
	assert_condition_includes("onbranch", "dir/subdir/", true);
	assert_condition_includes("onbranch", "dir/subdir/*", true);
	assert_condition_includes("onbranch", "dir/subdir/ref", true);
	assert_condition_includes("onbranch", "dir/", true);
	assert_condition_includes("onbranch", "dir/**", true);
	assert_condition_includes("onbranch", "**", true);
	assert_condition_includes("onbranch", "dir", false);
	assert_condition_includes("onbranch", "dir*", false);
	assert_condition_includes("onbranch", "dir/*", false);
}

void test_config_conditionals__empty(void)
{
	git_buf value = GIT_BUF_INIT;
	git_str buf = GIT_STR_INIT;
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, _repo));
	cl_git_pass(git_config_set_string(cfg, "includeIf.path", "other"));
	git_config_free(cfg);

	cl_git_mkfile("empty_standard_repo/.git/other", "[foo]\nbar=baz\n");
	_repo = cl_git_sandbox_reopen();

	git_str_dispose(&buf);

	cl_git_pass(git_repository_config(&cfg, _repo));

	cl_git_fail_with(GIT_ENOTFOUND,
		git_config_get_string_buf(&value, cfg, "foo.bar"));

	git_str_dispose(&buf);
	git_buf_dispose(&value);
	git_config_free(cfg);
}
