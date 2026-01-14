#include "clar_libgit2.h"
#include "remote.h"
#include "repository.h"

#define REPO_PATH "testrepo2/.gitted"
#define REMOTE_ORIGIN "origin"
#define REMOTE_INSTEADOF_URL_FETCH "insteadof-url-fetch"
#define REMOTE_INSTEADOF_URL_PUSH "insteadof-url-push"
#define REMOTE_INSTEADOF_URL_BOTH "insteadof-url-both"
#define REMOTE_INSTEADOF_PUSHURL_FETCH "insteadof-pushurl-fetch"
#define REMOTE_INSTEADOF_PUSHURL_PUSH "insteadof-pushurl-push"
#define REMOTE_INSTEADOF_PUSHURL_BOTH "insteadof-pushurl-both"

static git_repository *g_repo;
static git_remote *g_remote;

void test_remote_insteadof__initialize(void)
{
	g_repo = NULL;
	g_remote = NULL;
}

void test_remote_insteadof__cleanup(void)
{
	git_repository_free(g_repo);
	git_remote_free(g_remote);
}

void test_remote_insteadof__not_applicable(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, REMOTE_ORIGIN));

	cl_assert_equal_s(
		git_remote_url(g_remote),
		"https://github.com/libgit2/false.git");
	cl_assert_equal_p(git_remote_pushurl(g_remote), NULL);
}

void test_remote_insteadof__url_insteadof_fetch(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, REMOTE_INSTEADOF_URL_FETCH));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://github.com/url/fetch/libgit2");
	cl_assert_equal_p(git_remote_pushurl(g_remote), NULL);
}

void test_remote_insteadof__url_insteadof_push(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, REMOTE_INSTEADOF_URL_PUSH));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://example.com/url/push/libgit2");
	cl_assert_equal_s(
	    git_remote_pushurl(g_remote),
	    "git@github.com:url/push/libgit2");
}

void test_remote_insteadof__url_insteadof_both(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, REMOTE_INSTEADOF_URL_BOTH));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://github.com/url/both/libgit2");
	cl_assert_equal_s(
	    git_remote_pushurl(g_remote),
	    "git@github.com:url/both/libgit2");
}

void test_remote_insteadof__pushurl_insteadof_fetch(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, REMOTE_INSTEADOF_PUSHURL_FETCH));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://github.com/url/fetch/libgit2");
	cl_assert_equal_s(
	    git_remote_pushurl(g_remote),
	    "http://github.com/url/fetch/libgit2-push");
}

void test_remote_insteadof__pushurl_insteadof_push(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, REMOTE_INSTEADOF_PUSHURL_PUSH));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://example.com/url/push/libgit2");
	cl_assert_equal_s(
	    git_remote_pushurl(g_remote),
	    "http://example.com/url/push/libgit2-push");
}

void test_remote_insteadof__pushurl_insteadof_both(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, REMOTE_INSTEADOF_PUSHURL_BOTH));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://github.com/url/both/libgit2");
	cl_assert_equal_s(
	    git_remote_pushurl(g_remote),
	    "http://github.com/url/both/libgit2-push");
}

void test_remote_insteadof__anonymous_remote_fetch(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_create_anonymous(&g_remote, g_repo,
	    "http://example.com/url/fetch/libgit2"));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://github.com/url/fetch/libgit2");
	cl_assert_equal_p(git_remote_pushurl(g_remote), NULL);
}

void test_remote_insteadof__anonymous_remote_push(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_create_anonymous(&g_remote, g_repo,
	    "http://example.com/url/push/libgit2"));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://example.com/url/push/libgit2");
	cl_assert_equal_s(
	    git_remote_pushurl(g_remote),
	    "git@github.com:url/push/libgit2");
}

void test_remote_insteadof__anonymous_remote_both(void)
{
	cl_git_pass(git_repository_open(&g_repo, cl_fixture(REPO_PATH)));
	cl_git_pass(git_remote_create_anonymous(&g_remote, g_repo,
	    "http://example.com/url/both/libgit2"));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://github.com/url/both/libgit2");
	cl_assert_equal_s(
	    git_remote_pushurl(g_remote),
	    "git@github.com:url/both/libgit2");
}

void test_remote_insteadof__detached_remote_fetch_insteadof(void)
{
	git_config *cfg;

	cl_fake_globalconfig(NULL);

	cl_git_pass(git_config_open_default(&cfg));
	cl_git_pass(git_config_set_string(cfg, "url.http://github.com/url/fetch.insteadOf",
	    "http://example.com/url/fetch"));
	git_config_free(cfg);

	cl_git_pass(git_remote_create_detached(&g_remote,
	    "http://example.com/url/fetch/libgit2"));

	/*
	 * TODO: this should be "http://github.com/url/fetch/libgit2" once
	 * git_remote_create_detached applies insteadOf from global config.
	 * See: https://github.com/libgit2/libgit2/issues/5469
	 */
	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://example.com/url/fetch/libgit2");
	cl_assert_equal_p(git_remote_pushurl(g_remote), NULL);
}

void test_remote_insteadof__detached_remote_push_insteadof(void)
{
	git_config *cfg;

	cl_fake_globalconfig(NULL);

	cl_git_pass(git_config_open_default(&cfg));
	cl_git_pass(git_config_set_string(cfg, "url.git@github.com:url/push.pushInsteadOf",
	    "http://example.com/url/push"));
	git_config_free(cfg);

	cl_git_pass(git_remote_create_detached(&g_remote,
	    "http://example.com/url/push/libgit2"));

	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://example.com/url/push/libgit2");
	/*
	 * TODO: this should be "git@github.com:url/push/libgit2" once
	 * git_remote_create_detached applies pushInsteadOf from global config.
	 * See: https://github.com/libgit2/libgit2/issues/5469
	 */
	cl_assert_equal_p(git_remote_pushurl(g_remote), NULL);
}

void test_remote_insteadof__detached_remote_both_insteadof(void)
{
	git_config *cfg;

	cl_fake_globalconfig(NULL);

	cl_git_pass(git_config_open_default(&cfg));
	cl_git_pass(git_config_set_string(cfg, "url.http://github.com/url/both.insteadOf",
	    "http://example.com/url/both"));
	cl_git_pass(git_config_set_string(cfg, "url.git@github.com:url/both.pushInsteadOf",
	    "http://example.com/url/both"));
	git_config_free(cfg);

	cl_git_pass(git_remote_create_detached(&g_remote,
	    "http://example.com/url/both/libgit2"));

	/*
	 * TODO: these should be the rewritten URLs once
	 * git_remote_create_detached applies insteadOf from global config.
	 * See: https://github.com/libgit2/libgit2/issues/5469
	 */
	cl_assert_equal_s(
	    git_remote_url(g_remote),
	    "http://example.com/url/both/libgit2");
	cl_assert_equal_p(git_remote_pushurl(g_remote), NULL);
}
