#include "clar_libgit2.h"

#include "path.h"

static git_repository *g_repo;
static git_buf g_buf;

void test_submodule_resolve__initialize(void)
{
	cl_git_pass(git_buf_init(&g_buf, 0));
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_submodule_resolve__cleanup(void)
{
	git_buf_dispose(&g_buf);
	cl_git_sandbox_cleanup();
}

static void assert_resolves(const char *remote_url, const char *url, const char *expected)
{
	git_buf resolved = GIT_BUF_INIT;
	git_remote *remote = NULL;

	if (remote_url)
		cl_git_pass(git_remote_create(&remote, g_repo, "origin", remote_url));
	cl_git_pass(git_submodule_resolve_url(&resolved, g_repo, url));
	cl_assert_equal_s(resolved.ptr, expected);

	git_remote_free(remote);
	git_buf_dispose(&resolved);
}

void test_submodule_resolve__absolute_path(void)
{
	assert_resolves(NULL, "/foo/bar", "/foo/bar");
}

void test_submodule_resolve__relative_dot(void)
{
	assert_resolves(NULL, "./", git_repository_workdir(g_repo));
}

void test_submodule_resolve__relative_child(void)
{
	cl_git_pass(git_buf_printf(&g_buf, "%schild", git_repository_workdir(g_repo)));
	assert_resolves(NULL, "./child", g_buf.ptr);
}

void test_submodule_resolve__relative_sibling(void)
{
	cl_assert(git_path_dirname_r(&g_buf, git_repository_workdir(g_repo)) > 0);
	cl_git_pass(git_buf_puts(&g_buf, "/sibling"));
	assert_resolves(NULL, "../sibling", g_buf.ptr);
}

void test_submodule_resolve__absolute_path_with_http_remote(void)
{
	assert_resolves("https://example.com/foobar", "/foo/bar", "/foo/bar");
}

void test_submodule_resolve__relative_dot_with_http_remote(void)
{
	assert_resolves("https://example.com/foobar", "./", "https://example.com/foobar/");
}

void test_submodule_resolve__relative_child_with_http_remote(void)
{
	assert_resolves("https://example.com/foobar", "./child", "https://example.com/foobar/child");
}

void test_submodule_resolve__relative_sibling_with_http_remote(void)
{
	assert_resolves("https://example.com/foobar", "../sibling", "https://example.com/sibling");
}

void test_submodule_resolve__absolute_path_with_ssh_remote(void)
{
	assert_resolves("git@example.com:foobar", "/foo/bar", "/foo/bar");
}

void test_submodule_resolve__relative_dot_with_ssh_remote(void)
{
	assert_resolves("git@example.com:foobar", "./", "git@example.com:foobar/");
}

void test_submodule_resolve__relative_child_with_ssh_remote(void)
{
	assert_resolves("git@example.com:foobar", "./child", "git@example.com:foobar/child");
}

void test_submodule_resolve__relative_sibling_with_ssh_remote(void)
{
	assert_resolves("git@example.com:foobar", "../sibling", "git@example.com:sibling");
}

void test_submodule_resolve__absolute_path_with_ssh_schema_remote(void)
{
	assert_resolves("ssh://git@example.com:foobar", "/foo/bar", "/foo/bar");
}

void test_submodule_resolve__relative_dot_with_ssh_schema_remote(void)
{
	assert_resolves("ssh://git@example.com:foobar", "./", "ssh://git@example.com:foobar/");
}

void test_submodule_resolve__relative_child_with_ssh_schema_remote(void)
{
	assert_resolves("ssh://git@example.com:foobar", "./child", "ssh://git@example.com:foobar/child");
}

void test_submodule_resolve__relative_sibling_with_ssh_schema_remote(void)
{
	assert_resolves("ssh://git@example.com:foobar", "../sibling", "ssh://git@example.com:sibling");
}
