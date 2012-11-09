#include "clar_libgit2.h"

#include "buffer.h"
#include "path.h"
#include "remote.h"

static void build_local_file_url(git_buf *out, const char *fixture)
{
	const char *in_buf;

	git_buf path_buf = GIT_BUF_INIT;

	cl_git_pass(git_path_prettify_dir(&path_buf, fixture, NULL));
	cl_git_pass(git_buf_puts(out, "file://"));

#ifdef _MSC_VER
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

static void transfer_cb(const git_transfer_progress *stats, void *payload)
{
	int *callcount = (int*)payload;
	GIT_UNUSED(stats);
	(*callcount)++;
}

void test_network_fetchlocal__complete(void)
{
	git_buf url = GIT_BUF_INIT;
	git_repository *repo;
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};

	build_local_file_url(&url, cl_fixture("testrepo.git"));
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_remote_add(&origin, repo, GIT_REMOTE_ORIGIN, git_buf_cstr(&url)));
	cl_git_pass(git_remote_connect(origin, GIT_DIR_FETCH));
	cl_git_pass(git_remote_download(origin, transfer_cb, &callcount));
	cl_git_pass(git_remote_update_tips(origin));

	cl_git_pass(git_reference_list(&refnames, repo, GIT_REF_LISTALL));
	cl_assert_equal_i(18, refnames.count);
	cl_assert(callcount > 0);

	git_strarray_free(&refnames);
	git_remote_free(origin);
	git_repository_free(repo);
}

void test_network_fetchlocal__partial(void)
{
	git_repository *repo = cl_git_sandbox_init("partial-testrepo");
	git_buf url = GIT_BUF_INIT;
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};

	cl_git_pass(git_reference_list(&refnames, repo, GIT_REF_LISTALL));
	cl_assert_equal_i(1, refnames.count);

	build_local_file_url(&url, cl_fixture("testrepo.git"));
	cl_git_pass(git_remote_add(&origin, repo, GIT_REMOTE_ORIGIN, git_buf_cstr(&url)));
	cl_git_pass(git_remote_connect(origin, GIT_DIR_FETCH));
	cl_git_pass(git_remote_download(origin, transfer_cb, &callcount));
	cl_git_pass(git_remote_update_tips(origin));

	cl_git_pass(git_reference_list(&refnames, repo, GIT_REF_LISTALL));
	cl_assert_equal_i(19, refnames.count); /* 18 remote + 1 local */
	cl_assert(callcount > 0);

	git_strarray_free(&refnames);
	git_remote_free(origin);

	cl_git_sandbox_cleanup();
}
