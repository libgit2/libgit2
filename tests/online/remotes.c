#include "clar_libgit2.h"

#define URL "git://github.com/libgit2/TestGitRepository"
#define REFSPEC "refs/heads/first-merge:refs/remotes/origin/first-merge"

static int remote_single_branch(git_remote **out, git_repository *repo, const char *name, const char *url, void *payload)
{
	GIT_UNUSED(payload);

	cl_git_pass(git_remote_create_with_fetchspec(out, repo, name, url, REFSPEC));

	return 0;
}

void test_online_remotes__single_branch(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *repo;
	git_remote *remote;
	git_strarray refs;
	size_t i, count = 0;

	opts.remote_cb = remote_single_branch;
	opts.checkout_branch = "first-merge";

	cl_git_pass(git_clone(&repo, URL, "./single-branch", &opts));
	cl_git_pass(git_reference_list(&refs, repo));

	for (i = 0; i < refs.count; i++) {
		if (!git__prefixcmp(refs.strings[i], "refs/heads/"))
			count++;
	}
	cl_assert_equal_i(1, count);

	git_strarray_free(&refs);

	cl_git_pass(git_remote_lookup(&remote, repo, "origin"));
	cl_git_pass(git_remote_get_fetch_refspecs(&refs, remote));

	cl_assert_equal_i(1, refs.count);
	cl_assert_equal_s(REFSPEC, refs.strings[0]);

	git_strarray_free(&refs);
	git_remote_free(remote);
	git_repository_free(repo);
}

void test_online_remotes__restricted_refspecs(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *repo;

	opts.remote_cb = remote_single_branch;

	cl_git_fail_with(GIT_EINVALIDSPEC, git_clone(&repo, URL, "./restrict-refspec", &opts));
}

void test_online_remotes__detached_remote_fails_downloading(void)
{
	git_remote *remote;

	cl_git_pass(git_remote_create_detached(&remote, URL));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	cl_git_fail(git_remote_download(remote, NULL, NULL));

	git_remote_free(remote);
}

void test_online_remotes__detached_remote_fails_uploading(void)
{
	git_remote *remote;

	cl_git_pass(git_remote_create_detached(&remote, URL));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	cl_git_fail(git_remote_upload(remote, NULL, NULL));

	git_remote_free(remote);
}

void test_online_remotes__detached_remote_fails_pushing(void)
{
	git_remote *remote;

	cl_git_pass(git_remote_create_detached(&remote, URL));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	cl_git_fail(git_remote_push(remote, NULL, NULL));

	git_remote_free(remote);
}
