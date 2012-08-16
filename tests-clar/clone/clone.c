#include "clar_libgit2.h"

#include "git2/clone.h"
#include "repository.h"

#define DO_LOCAL_TEST 0
#define DO_LIVE_NETWORK_TESTS 0
#define LIVE_REPO_URL "http://github.com/libgit2/node-gitteh"


static git_repository *g_repo;

void test_clone_clone__initialize(void)
{
	g_repo = NULL;
}

void test_clone_clone__cleanup(void)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}
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
	cl_git_pass(git_clone(&g_repo, git_buf_cstr(&src), "./local", NULL, NULL, NULL));
	git_repository_free(g_repo);
	git_futils_rmdir_r("./local", GIT_DIRREMOVAL_FILES_AND_DIRS);
	cl_git_pass(git_clone_bare(&g_repo, git_buf_cstr(&src), "./local.git", NULL));
	git_futils_rmdir_r("./local.git", GIT_DIRREMOVAL_FILES_AND_DIRS);
#endif

	git_buf_free(&src);
}


void test_clone_clone__network_full(void)
{
#if DO_LIVE_NETWORK_TESTS
	git_remote *origin;

	cl_git_pass(git_clone(&g_repo, LIVE_REPO_URL, "./test2", NULL, NULL, NULL));
	cl_assert(!git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));
	git_futils_rmdir_r("./test2", GIT_DIRREMOVAL_FILES_AND_DIRS);
#endif
}

void test_clone_clone__network_bare(void)
{
#if DO_LIVE_NETWORK_TESTS
	git_remote *origin;

	cl_git_pass(git_clone_bare(&g_repo, LIVE_REPO_URL, "test", NULL));
	cl_assert(git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));
	git_futils_rmdir_r("./test", GIT_DIRREMOVAL_FILES_AND_DIRS);
#endif
}


void test_clone_clone__already_exists(void)
{
#if DO_LIVE_NETWORK_TESTS
	/* Should pass with existing-but-empty dir */
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_pass(git_clone(&g_repo, LIVE_REPO_URL, "./foo", NULL, NULL, NULL));
	git_repository_free(g_repo); g_repo = NULL;
	git_futils_rmdir_r("./foo", GIT_DIRREMOVAL_FILES_AND_DIRS);
#endif

	/* Should fail with a file */
	cl_git_mkfile("./foo", "Bar!");
	cl_git_fail(git_clone(&g_repo, LIVE_REPO_URL, "./foo", NULL, NULL, NULL));
	git_futils_rmdir_r("./foo", GIT_DIRREMOVAL_FILES_AND_DIRS);

	/* Should fail with existing-and-nonempty dir */
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_mkfile("./foo/bar", "Baz!");
	cl_git_fail(git_clone(&g_repo, LIVE_REPO_URL, "./foo", NULL, NULL, NULL));
	git_futils_rmdir_r("./foo", GIT_DIRREMOVAL_FILES_AND_DIRS);
}
