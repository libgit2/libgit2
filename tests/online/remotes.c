#include "clar_libgit2.h"

static int remote_single_branch(git_remote **out, git_repository *repo, const char *name, const char *url, void *payload)
{
	char *fetch_refspecs[] = {
		"refs/heads/first-merge:refs/remotes/origin/first-merge",
	};
	git_strarray fetch_refspecs_strarray = {
		fetch_refspecs,
		1,
	};

	GIT_UNUSED(payload);

	cl_git_pass(git_remote_create(out, repo, name, url));
	cl_git_pass(git_remote_set_fetch_refspecs(*out, &fetch_refspecs_strarray));

	return 0;
}

void test_online_remotes__single_branch(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *repo;
	git_strarray refs;
	size_t i, count = 0;

	opts.remote_cb = remote_single_branch;
	opts.checkout_branch = "first-merge";

	cl_git_pass(git_clone(&repo, "git://github.com/libgit2/TestGitRepository", "./single-branch", &opts));
	cl_git_pass(git_reference_list(&refs, repo));

	for (i = 0; i < refs.count; i++) {
		if (!git__prefixcmp(refs.strings[i], "refs/heads/"))
			count++;
	}
	cl_assert_equal_i(1, count);

	git_strarray_free(&refs);
	git_repository_free(repo);
}

void test_online_remotes__restricted_refspecs(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *repo;

	opts.remote_cb = remote_single_branch;

	cl_git_fail_with(GIT_EINVALIDSPEC, git_clone(&repo, "git://github.com/libgit2/TestGitRepository", "./restrict-refspec", &opts));
}
