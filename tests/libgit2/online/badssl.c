#include "clar_libgit2.h"

#include "git2/clone.h"

static git_repository *g_repo;

#ifdef GIT_HTTPS
static bool g_has_ssl = true;
#else
static bool g_has_ssl = false;
#endif

static int cert_check_assert_invalid(git_cert *cert, int valid, const char* host, void *payload)
{
	GIT_UNUSED(cert); GIT_UNUSED(host); GIT_UNUSED(payload);

	cl_assert_equal_i(0, valid);

	return GIT_ECERTIFICATE;
}

void test_online_badssl__expired(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.fetch_opts.callbacks.certificate_check = cert_check_assert_invalid;

	if (!g_has_ssl)
		cl_skip();

	cl_git_fail_with(GIT_ECERTIFICATE,
			 git_clone(&g_repo, "https://expired.badssl.com/fake.git", "./fake", NULL));

	cl_git_fail_with(GIT_ECERTIFICATE,
			 git_clone(&g_repo, "https://expired.badssl.com/fake.git", "./fake", &opts));
}

void test_online_badssl__wrong_host(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.fetch_opts.callbacks.certificate_check = cert_check_assert_invalid;

	if (!g_has_ssl)
		cl_skip();

	cl_git_fail_with(GIT_ECERTIFICATE,
			 git_clone(&g_repo, "https://wrong.host.badssl.com/fake.git", "./fake", NULL));
	cl_git_fail_with(GIT_ECERTIFICATE,
			 git_clone(&g_repo, "https://wrong.host.badssl.com/fake.git", "./fake", &opts));
}

void test_online_badssl__self_signed(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.fetch_opts.callbacks.certificate_check = cert_check_assert_invalid;

	if (!g_has_ssl)
		cl_skip();

	cl_git_fail_with(GIT_ECERTIFICATE,
			 git_clone(&g_repo, "https://self-signed.badssl.com/fake.git", "./fake", NULL));
	cl_git_fail_with(GIT_ECERTIFICATE,
			 git_clone(&g_repo, "https://self-signed.badssl.com/fake.git", "./fake", &opts));
}

void test_online_badssl__old_cipher(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.fetch_opts.callbacks.certificate_check = cert_check_assert_invalid;

	if (!g_has_ssl)
		cl_skip();

	cl_git_fail(git_clone(&g_repo, "https://rc4.badssl.com/fake.git", "./fake", NULL));
	cl_git_fail(git_clone(&g_repo, "https://rc4.badssl.com/fake.git", "./fake", &opts));
}

void test_online_badssl__untrusted(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.fetch_opts.callbacks.certificate_check = cert_check_assert_invalid;

	if (!g_has_ssl)
		cl_skip();

	cl_git_fail_with(
	        GIT_ECERTIFICATE,
	        git_clone(
	                &g_repo, "https://untrusted-root.badssl.com/fake.git",
	                "./fake", NULL));
	cl_assert_equal_i(git_error_last()->klass, GIT_ERROR_SSL);
	const char *message = git_error_last()->message;
	cl_assert(strstr(message, "certificate is not trusted") != NULL);
	cl_assert(
	        strstr(message,
	               "certificate revocation status could not be verified") !=
	        NULL);
	cl_assert(
	        strstr(message, "certificate revocation is offline or stale") !=
	        NULL);
}
