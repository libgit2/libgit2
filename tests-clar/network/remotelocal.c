#include "clar_libgit2.h"
#include "transport.h"
#include "buffer.h"
#include "path.h"
#include "posix.h"

static git_repository *repo;
static git_buf file_path_buf = GIT_BUF_INIT;
static git_remote *remote;

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

void test_network_remotelocal__initialize(void)
{
	cl_git_pass(git_repository_init(&repo, "remotelocal/", 0));
	cl_assert(repo != NULL);
}

void test_network_remotelocal__cleanup(void)
{
	git_remote_free(remote);
	git_buf_free(&file_path_buf);
	git_repository_free(repo);
	cl_fixture_cleanup("remotelocal");
}

static int count_ref__cb(git_remote_head *head, void *payload)
{
	int *count = (int *)payload;

	(void)head;
	(*count)++;

	return 0;
}

static int ensure_peeled__cb(git_remote_head *head, void *payload)
{
	GIT_UNUSED(payload);

	if(strcmp(head->name, "refs/tags/test^{}") != 0)
		return 0;

	return git_oid_streq(&head->oid, "e90810b8df3e80c413d903f631643c716887138d");
}

static void connect_to_local_repository(const char *local_repository)
{
	build_local_file_url(&file_path_buf, local_repository);

	cl_git_pass(git_remote_new(&remote, repo, NULL, git_buf_cstr(&file_path_buf), NULL));
	cl_git_pass(git_remote_connect(remote, GIT_DIR_FETCH));

}

void test_network_remotelocal__retrieve_advertised_references(void)
{
	int how_many_refs = 0;

	connect_to_local_repository(cl_fixture("testrepo.git"));

	cl_git_pass(git_remote_ls(remote, &count_ref__cb, &how_many_refs));

	cl_assert(how_many_refs == 14); /* 1 HEAD + 6 heads + 1 lightweight tag + 3 annotated tags + 3 peeled target */
}

void test_network_remotelocal__retrieve_advertised_references_from_spaced_repository(void)
{
	int how_many_refs = 0;

	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(p_rename("testrepo.git", "spaced testrepo.git"));

	connect_to_local_repository("spaced testrepo.git");

	cl_git_pass(git_remote_ls(remote, &count_ref__cb, &how_many_refs));

	cl_assert(how_many_refs == 14); /* 1 HEAD + 6 heads + 1 lightweight tag + 3 annotated tags + 3 peeled target */

	git_remote_free(remote);	/* Disconnect from the "spaced repo" before the cleanup */
	remote = NULL;

	cl_fixture_cleanup("spaced testrepo.git");
}

void test_network_remotelocal__nested_tags_are_completely_peeled(void)
{
	connect_to_local_repository(cl_fixture("testrepo.git"));

	cl_git_pass(git_remote_ls(remote, &ensure_peeled__cb, NULL));
}
