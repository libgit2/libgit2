#include "clar_libgit2.h"

#define GH_REPO_SSH_URL "ssh://github.com/libgit2/TestGitRepository"
#define GH_REPO_SSH_USER_URL "ssh://git@github.com/libgit2/TestGitRepository"

#define CLONE_PATH "./foo"

static git_repository *g_repo;
static git_clone_options g_options;

void test_online_clone_ssh__initialize(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
#ifndef GIT_SSH
	clar__skip();
#endif
	memcpy(&g_options, &opts, sizeof(g_options));
	g_repo = NULL;
}

void test_online_clone_ssh__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup(CLONE_PATH);
}

static int check_ssh_auth_methods(git_cred **cred, const char *url, const char *username_from_url,
				  unsigned int allowed_types, void *data)
{
	int *with_user = (int *) data;
	GIT_UNUSED(cred); GIT_UNUSED(url); GIT_UNUSED(username_from_url); GIT_UNUSED(data);

	if (!*with_user)
		cl_assert_equal_i(GIT_CREDTYPE_USERNAME, allowed_types);
	else
		cl_assert(!(allowed_types & GIT_CREDTYPE_USERNAME));

	return GIT_EUSER;
}

void test_online_clone_ssh__ssh_auth_methods(void)
{
	int with_user;

	g_options.fetch_opts.callbacks.credentials = check_ssh_auth_methods;
	g_options.fetch_opts.callbacks.payload = &with_user;
	g_options.fetch_opts.callbacks.certificate_check = NULL;

	with_user = 0;
	cl_git_fail_with(GIT_EUSER, git_clone(&g_repo, GH_REPO_SSH_URL, CLONE_PATH, &g_options));

	with_user = 1;
	cl_git_fail_with(GIT_EUSER, git_clone(&g_repo, GH_REPO_SSH_USER_URL, CLONE_PATH, &g_options));
}

static int fail_certificate_check(git_cert *cert, int valid, const char *host, void *payload)
{
	GIT_UNUSED(cert);
	GIT_UNUSED(valid);
	GIT_UNUSED(host);
	GIT_UNUSED(payload);
	return GIT_ECERTIFICATE;
}

void test_online_clone_ssh__certificate_invalid(void)
{
	g_options.fetch_opts.callbacks.certificate_check = fail_certificate_check;
	cl_git_fail_with(git_clone(&g_repo, GH_REPO_SSH_URL, CLONE_PATH, &g_options),
		GIT_ECERTIFICATE);
}

static int cred_foo_bar(git_cred **cred, const char *url, const char *username_from_url,
				  unsigned int allowed_types, void *data)

{
	GIT_UNUSED(url); GIT_UNUSED(username_from_url); GIT_UNUSED(allowed_types); GIT_UNUSED(data);

	return git_cred_userpass_plaintext_new(cred, "foo", "bar");
}

void test_online_clone_ssh__ssh_cannot_change_username(void)
{
	g_options.fetch_opts.callbacks.credentials = cred_foo_bar;

	cl_git_fail(git_clone(&g_repo, GH_REPO_SSH_USER_URL, CLONE_PATH, &g_options));
}
