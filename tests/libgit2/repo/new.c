#include "clar_libgit2.h"
#include "git2/sys/repository.h"

void test_repo_new__has_nothing(void)
{
	git_repository *repo;

#ifdef GIT_EXPERIMENTAL_SHA256
	git_repository_new_options repo_opts = GIT_REPOSITORY_NEW_OPTIONS_INIT;

	repo_opts.oid_type = GIT_OID_SHA1;

	cl_git_pass(git_repository_new(&repo, &repo_opts));
#else
	cl_git_pass(git_repository_new(&repo));
#endif
	cl_assert_equal_b(true, git_repository_is_bare(repo));
	cl_assert_equal_p(NULL, git_repository_path(repo));
	cl_assert_equal_p(NULL, git_repository_workdir(repo));
	git_repository_free(repo);
}

void test_repo_new__is_bare_until_workdir_set(void)
{
	git_repository *repo;

#ifdef GIT_EXPERIMENTAL_SHA256
	git_repository_new_options repo_opts = GIT_REPOSITORY_NEW_OPTIONS_INIT;

	repo_opts.oid_type = GIT_OID_SHA1;

	cl_git_pass(git_repository_new(&repo, &repo_opts));
#else
	cl_git_pass(git_repository_new(&repo));
#endif
	cl_assert_equal_b(true, git_repository_is_bare(repo));

	cl_git_pass(git_repository_set_workdir(repo, clar_sandbox_path(), 0));
	cl_assert_equal_b(false, git_repository_is_bare(repo));

	git_repository_free(repo);
}

void test_repo_new__sha1(void)
{
	git_repository *repo;

#ifdef GIT_EXPERIMENTAL_SHA256
	git_repository_new_options repo_opts = GIT_REPOSITORY_NEW_OPTIONS_INIT;

	repo_opts.oid_type = GIT_OID_SHA1;

	cl_git_pass(git_repository_new(&repo, &repo_opts));
#else
	cl_git_pass(git_repository_new(&repo));
#endif
	cl_assert_equal_i(GIT_OID_SHA1, git_repository_oid_type(repo));

	git_repository_free(repo);
}

void test_repo_new__sha256(void)
{
#ifndef GIT_EXPERIMENTAL_SHA256
	cl_skip();
#else
	git_repository *repo;
	git_repository_new_options repo_opts = GIT_REPOSITORY_NEW_OPTIONS_INIT;

	repo_opts.oid_type = GIT_OID_SHA256;

	cl_git_pass(git_repository_new(&repo, &repo_opts));
	cl_assert_equal_i(GIT_OID_SHA256, git_repository_oid_type(repo));

	git_repository_free(repo);
#endif
}
