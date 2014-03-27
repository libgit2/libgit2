/* test the submodule APIs on repositories where there are no submodules */

#include "clar_libgit2.h"
#include "posix.h"

void test_submodule_nosubs__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_submodule_nosubs__lookup(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_submodule *sm = NULL;

	p_mkdir("status/subrepo", 0777);
	cl_git_mkfile("status/subrepo/.git", "gitdir: ../.git");

	cl_assert_equal_i(GIT_ENOTFOUND, git_submodule_lookup(&sm, repo, "subdir"));

	cl_assert_equal_i(GIT_EEXISTS, git_submodule_lookup(&sm, repo, "subrepo"));

	cl_git_pass(git_submodule_reload_all(repo, 0));

	cl_assert_equal_i(GIT_ENOTFOUND, git_submodule_lookup(&sm, repo, "subdir"));

	cl_assert_equal_i(GIT_EEXISTS, git_submodule_lookup(&sm, repo, "subrepo"));
}

void test_submodule_nosubs__immediate_reload(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	cl_git_pass(git_submodule_reload_all(repo, 0));
}

static int fake_submod_cb(git_submodule *sm, const char *n, void *p)
{
	GIT_UNUSED(sm); GIT_UNUSED(n); GIT_UNUSED(p);
	return 0;
}

void test_submodule_nosubs__foreach(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	cl_git_pass(git_submodule_foreach(repo, fake_submod_cb, NULL));
}

void test_submodule_nosubs__add(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_submodule *sm, *sm2;

	cl_git_pass(git_submodule_add_setup(&sm, repo, "https://github.com/libgit2/libgit2.git", "submodules/libgit2", 1));

	cl_git_pass(git_submodule_lookup(&sm2, repo, "submodules/libgit2"));
	git_submodule_free(sm2);

	cl_git_pass(git_submodule_foreach(repo, fake_submod_cb, NULL));
	cl_git_pass(git_submodule_reload_all(repo, 0));

	git_submodule_free(sm);
}

void test_submodule_nosubs__reload_add_reload(void)
{
	git_repository *repo = cl_git_sandbox_init("status");
	git_submodule *sm;

	cl_git_pass(git_submodule_reload_all(repo, 0));

	cl_git_pass(git_submodule_add_setup(&sm, repo, "https://github.com/libgit2/libgit2.git", "submodules/libgit2", 1));

	cl_git_pass(git_submodule_reload_all(repo, 0));

	cl_assert_equal_s("submodules/libgit2", git_submodule_name(sm));
	git_submodule_free(sm);

	cl_git_pass(git_submodule_lookup(&sm, repo, "submodules/libgit2"));
	cl_assert_equal_s("submodules/libgit2", git_submodule_name(sm));
	git_submodule_free(sm);
}

void test_submodule_nosubs__bad_gitmodules(void)
{
	git_repository *repo = cl_git_sandbox_init("status");

	cl_git_mkfile("status/.gitmodules", "[submodule \"foobar\"]\tpath=blargle\n\turl=\n\tbranch=\n\tupdate=flooble\n\n");
	cl_git_fail(git_submodule_reload_all(repo, 0));

	cl_git_rewritefile("status/.gitmodules", "[submodule \"foobar\"]\tpath=blargle\n\turl=\n\tbranch=\n\tupdate=rebase\n\n");
	cl_git_pass(git_submodule_reload_all(repo, 0));

	cl_git_pass(git_submodule_lookup(NULL, repo, "foobar"));
	cl_assert_equal_i(GIT_ENOTFOUND, git_submodule_lookup(NULL, repo, "subdir"));
}
