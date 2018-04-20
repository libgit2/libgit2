#include "clar_libgit2.h"

#include "git2/clone.h"

static git_repository *g_repo;

#ifdef GIT_HTTPS
static bool g_has_ssl = true;
#else
static bool g_has_ssl = false;
#endif

#if defined(GIT_SSH)
static bool g_has_ssh = true;
#else
static bool g_has_ssh = false;
#endif

struct cert_options {
	int expect_valid;
	int had_callback;
	int return_code;
	char *expect_host;
};

static git_clone_options g_options;
struct cert_options g_cert_options;

void test_online_cert__initialize(void)
{
	git_checkout_options dummy_checkout = GIT_CHECKOUT_OPTIONS_INIT;
	git_fetch_options dummy_fetch = GIT_FETCH_OPTIONS_INIT;

	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
	g_options.checkout_opts = dummy_checkout;
	g_options.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	g_options.fetch_opts = dummy_fetch;

	memset(&g_cert_options, 0, sizeof(struct cert_options));
	g_options.fetch_opts.callbacks.payload = &g_cert_options;
}

void test_online_cert__cleanup(void)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}
	cl_fixture_cleanup("./fake");
}

static int certificate_check_cb(git_cert *cert, int valid, const char *host, void *payload)
{
	struct cert_options *options = payload;

	GIT_UNUSED(cert);

	if (options) {
		options->had_callback = 1;
		cl_assert_equal_i(valid, options->expect_valid);
		if (options->expect_host) {
			cl_assert_equal_s(host, options->expect_host);
		}
	}

	return (options ? options->return_code : GIT_ERROR);
}

void test_online_cert__passthrough_https(void)
{
	if (!g_has_ssl)
		cl_skip();

	g_options.fetch_opts.callbacks.certificate_check = certificate_check_cb;

	g_cert_options.expect_valid = 1;
	g_cert_options.return_code = GIT_PASSTHROUGH;

	cl_git_pass(git_clone(&g_repo, "https://github.com/libgit2/TestGitRepository", "./fake", &g_options));
}

void test_online_cert__passthrough_https_bad_certificate(void)
{
	if (!g_has_ssl)
		cl_skip();

	g_options.fetch_opts.callbacks.certificate_check = certificate_check_cb;

	g_cert_options.expect_valid = 0;
	g_cert_options.return_code = GIT_PASSTHROUGH;

	cl_git_fail_with(GIT_ECERTIFICATE,
					 git_clone(&g_repo, "https://wrong.host.badssl.com/fake.git", "./fake", &g_options));
}

void test_online_cert__passthrough_https_overriden(void)
{
	if (!g_has_ssl)
		cl_skip();

	g_options.fetch_opts.callbacks.certificate_check = certificate_check_cb;

	g_cert_options.expect_valid = 1;
	g_cert_options.return_code = -42;

	cl_git_fail_with(-42,
					 git_clone(&g_repo, "https://github.com/libgit2/TestGitRepository", "./fake", &g_options));
}

void test_online_cert__passthrough_https_overriden_success(void)
{
	if (!g_has_ssl)
		cl_skip();

	g_options.fetch_opts.callbacks.certificate_check = certificate_check_cb;

	g_cert_options.expect_valid = 0;
	g_cert_options.return_code = GIT_OK;

	/* Error because there's no repo to clone */
	cl_git_fail_with(GIT_ERROR,
					 git_clone(&g_repo, "https://wrong.host.badssl.com/fake.git", "./fake", &g_options));
}

void test_online_cert__passthrough_ssh(void)
{
	if (!g_has_ssh)
		cl_skip();

	g_options.fetch_opts.callbacks.certificate_check = certificate_check_cb;

	g_cert_options.expect_valid = 0;
	g_cert_options.return_code = GIT_PASSTHROUGH;

	/* Authentication required, no cb set */
	cl_git_fail_with(GIT_ERROR,
					 git_clone(&g_repo, "ssh://github.com/libgit2/TestGitRepository", "./fake", &g_options));
}
