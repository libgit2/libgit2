#include "clar_libgit2.h"

#include "git2/clone.h"
#include "repository.h"

#define DO_LOCAL_TEST 0
#define DO_LIVE_NETWORK_TESTS 0
#define LIVE_REPO_URL "git://github.com/nulltoken/TestGitRepository"
#define LIVE_EMPTYREPO_URL "git://github.com/nulltoken/TestEmptyRepository"


static git_repository *g_repo;

void test_clone_clone__initialize(void)
{
	g_repo = NULL;
}

static void cleanup_repository(void *path)
{
	if (g_repo)
		git_repository_free(g_repo);
	cl_fixture_cleanup((const char *)path);
}

// TODO: This is copy/pasted from network/remotelocal.c.
static void build_local_file_url(git_buf *out, const char *fixture)
{
	const char *in_buf;

	git_buf path_buf = GIT_BUF_INIT;

	cl_git_pass(git_path_prettify_dir(&path_buf, fixture, NULL));
	cl_git_pass(git_buf_puts(out, "file://"));

#ifdef GIT_WIN32
	/*
	 * A FILE uri matches the following format: file://[host]/path
	 * where "host" can be empty and "path" is an absolute path to the resource.
	 *
	 * In this test, no hostname is used, but we have to ensure the leading triple slashes:
	 *
	 * *nix: file:///usr/home/...
	 * Windows: file:///C:/Users/...
	 */
	cl_git_pass(git_buf_putc(out, '/'));
#endif

	in_buf = git_buf_cstr(&path_buf);

	/*
	 * A very hacky Url encoding that only takes care of escaping the spaces
	 */
	while (*in_buf) {
		if (*in_buf == ' ')
			cl_git_pass(git_buf_puts(out, "%20"));
		else
			cl_git_pass(git_buf_putc(out, *in_buf));

		in_buf++;
	}

	git_buf_free(&path_buf);
}

void test_clone_clone__bad_url(void)
{
	/* Clone should clean up the mess if the URL isn't a git repository */
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", NULL, NULL, NULL));
	cl_assert(!git_path_exists("./foo"));
	cl_git_fail(git_clone_bare(&g_repo, "not_a_repo", "./foo.git", NULL));
	cl_assert(!git_path_exists("./foo.git"));
}

void test_clone_clone__local(void)
{
	git_buf src = GIT_BUF_INIT;
	build_local_file_url(&src, cl_fixture("testrepo.git"));

#if DO_LOCAL_TEST
	cl_set_cleanup(&cleanup_repository, "./local");

	cl_git_pass(git_clone(&g_repo, git_buf_cstr(&src), "./local", NULL, NULL, NULL));
#endif

	git_buf_free(&src);
}

void test_clone_clone__local_bare(void)
{
	git_buf src = GIT_BUF_INIT;
	build_local_file_url(&src, cl_fixture("testrepo.git"));

#if DO_LOCAL_TEST
	cl_set_cleanup(&cleanup_repository, "./local.git");

	cl_git_pass(git_clone_bare(&g_repo, git_buf_cstr(&src), "./local.git", NULL));
#endif

	git_buf_free(&src);
}

void test_clone_clone__network_full(void)
{
#if DO_LIVE_NETWORK_TESTS
	git_remote *origin;

	cl_set_cleanup(&cleanup_repository, "./test2");

	cl_git_pass(git_clone(&g_repo, LIVE_REPO_URL, "./test2", NULL, NULL, NULL));
	cl_assert(!git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));
#endif
}

void test_clone_clone__network_bare(void)
{
#if DO_LIVE_NETWORK_TESTS
	git_remote *origin;

	cl_set_cleanup(&cleanup_repository, "./test");

	cl_git_pass(git_clone_bare(&g_repo, LIVE_REPO_URL, "./test", NULL));
	cl_assert(git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));
#endif
}

void test_clone_clone__cope_with_already_existing_directory(void)
{
#if DO_LIVE_NETWORK_TESTS
	cl_set_cleanup(&cleanup_repository, "./foo");

	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_pass(git_clone(&g_repo, LIVE_REPO_URL, "./foo", NULL, NULL, NULL));
	git_repository_free(g_repo); g_repo = NULL;
#endif
}

void test_clone_clone__fail_when_the_target_is_a_file(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	cl_git_mkfile("./foo", "Bar!");
	cl_git_fail(git_clone(&g_repo, LIVE_REPO_URL, "./foo", NULL, NULL, NULL));
}

void test_clone_clone__fail_with_already_existing_but_non_empty_directory(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_mkfile("./foo/bar", "Baz!");
	cl_git_fail(git_clone(&g_repo, LIVE_REPO_URL, "./foo", NULL, NULL, NULL));
}

void test_clone_clone__empty_repository(void)
{
#if DO_LIVE_NETWORK_TESTS
	git_reference *head;

	cl_set_cleanup(&cleanup_repository, "./empty");

	cl_git_pass(git_clone(&g_repo, LIVE_EMPTYREPO_URL, "./empty", NULL, NULL, NULL));

	cl_assert_equal_i(true, git_repository_is_empty(g_repo));
	cl_assert_equal_i(true, git_repository_head_orphan(g_repo));

	cl_git_pass(git_reference_lookup(&head, g_repo, GIT_HEAD_FILE));
	cl_assert_equal_i(GIT_REF_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s("refs/heads/master", git_reference_target(head));

	git_reference_free(head);
#endif
}
