#include "clar_libgit2.h"
#include "config/config_helpers.h"

static git_repository *_repo;

#define TEST_URL "http://github.com/libgit2/libgit2.git"

void test_remote_list__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo");
}

void test_remote_list__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_remote_list__always_checks_disk_config(void)
{
	git_repository *repo;
	git_strarray remotes;
	git_remote *remote;

	cl_git_pass(git_repository_open(&repo, git_repository_path(_repo)));

	cl_git_pass(git_remote_list(&remotes, _repo));
	cl_assert_equal_sz(remotes.count, 1);
	git_strarray_dispose(&remotes);

	cl_git_pass(git_remote_create(&remote, _repo, "valid-name", TEST_URL));

	cl_git_pass(git_remote_list(&remotes, _repo));
	cl_assert_equal_sz(remotes.count, 2);
	git_strarray_dispose(&remotes);

	cl_git_pass(git_remote_list(&remotes, repo));
	cl_assert_equal_sz(remotes.count, 2);
	git_strarray_dispose(&remotes);

	git_repository_free(repo);
	git_remote_free(remote);
}


void test_remote_list__includes_remotes_without_urls(void)
{
	git_config *config;
	git_remote *remote;
	git_strarray remotes;
	size_t i;
	int saw_unknown = 0, saw_fetch = 0;

	cl_git_pass(git_repository_config(&config, _repo));
	cl_git_pass(git_config_set_string(config, "remote.only-fetch.fetch", ""));
	cl_git_pass(git_config_set_string(config, "remote.with.dot.unknown", "yes"));
	cl_git_pass(git_config_set_string(config, "remote.with.dot.push", "refs/heads/main"));
	cl_git_pass(git_config_set_string(config, "remote.pushDefault", "unconfigured"));
	cl_git_pass(git_remote_list(&remotes, _repo));
	cl_assert_equal_sz(remotes.count, 3);
	for (i = 0; i < remotes.count; i++) {
		saw_unknown += !strcmp(remotes.strings[i], "with.dot");
		saw_fetch += !strcmp(remotes.strings[i], "only-fetch");
		cl_git_pass(git_remote_lookup(&remote, _repo, remotes.strings[i]));
		git_remote_free(remote);
	}
	cl_assert_equal_i(saw_unknown, 1);
	cl_assert_equal_i(saw_fetch, 1);
	git_strarray_dispose(&remotes);
	git_config_free(config);
}
