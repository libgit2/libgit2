#include "clar_libgit2.h"

#define REPO_URL "http://github.com/libgit2/TestGitRepository"

static git_repository *g_repo;
static git_clone_options g_options;

static char *_remote_sslnoverify;
static char *_remote_proxy_scheme;
static char *_remote_proxy_host;
static char *_remote_proxy_user;
static char *_remote_proxy_pass;
static char *_remote_proxy_selfsigned;

static int _orig_proxies_need_reset;
static char *_orig_http_proxy;
static char *_orig_https_proxy;

static int ssl_cert(git_cert *cert, int valid, const char *host, void *payload)
{
	GIT_UNUSED(cert);
	GIT_UNUSED(host);
	GIT_UNUSED(payload);

	if (_remote_sslnoverify != NULL)
		valid = 1;

	return valid ? 0 : GIT_ECERTIFICATE;
}

static int proxy_cert_cb(git_cert *cert, int valid, const char *host, void *payload)
{
	char *colon;
	size_t host_len;

	GIT_UNUSED(cert);
	GIT_UNUSED(valid);
	GIT_UNUSED(payload);

	cl_assert(_remote_proxy_host);

	if ((colon = strchr(_remote_proxy_host, ':')) != NULL)
		host_len = (colon - _remote_proxy_host);
	else
		host_len = strlen(_remote_proxy_host);

	if (_remote_proxy_selfsigned != NULL &&
	    strlen(host) == host_len &&
	    strncmp(_remote_proxy_host, host, host_len) == 0)
		valid = 1;

	return valid ? 0 : GIT_ECERTIFICATE;
}

static int proxy_cred_cb(git_cred **out, const char *url, const char *username, unsigned int allowed, void *payload)
{
	int *called_proxy_creds = (int *) payload;
	GIT_UNUSED(url);
	GIT_UNUSED(username);
	GIT_UNUSED(allowed);
	*called_proxy_creds = 1;
	return git_cred_userpass_plaintext_new(out, _remote_proxy_user, _remote_proxy_pass);
}

void test_online_proxy__initialize(void)
{
	git_checkout_options dummy_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_fetch_options dummy_fetch = GIT_FETCH_OPTIONS_INIT;

	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
	g_options.checkout_opts = dummy_opts;
	g_options.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	g_options.fetch_opts = dummy_fetch;
	g_options.fetch_opts.callbacks.certificate_check = ssl_cert;

	_remote_sslnoverify = cl_getenv("GITTEST_REMOTE_SSL_NOVERIFY");
	_remote_proxy_scheme = cl_getenv("GITTEST_REMOTE_PROXY_SCHEME");
	_remote_proxy_host = cl_getenv("GITTEST_REMOTE_PROXY_HOST");
	_remote_proxy_user = cl_getenv("GITTEST_REMOTE_PROXY_USER");
	_remote_proxy_pass = cl_getenv("GITTEST_REMOTE_PROXY_PASS");
	_remote_proxy_selfsigned = cl_getenv("GITTEST_REMOTE_PROXY_SELFSIGNED");

	_orig_proxies_need_reset = 0;
}

void test_online_proxy__cleanup(void)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}
	cl_fixture_cleanup("./foo");

	git__free(_remote_sslnoverify);
	git__free(_remote_proxy_scheme);
	git__free(_remote_proxy_host);
	git__free(_remote_proxy_user);
	git__free(_remote_proxy_pass);
	git__free(_remote_proxy_selfsigned);

	if (_orig_proxies_need_reset) {
		cl_setenv("HTTP_PROXY", _orig_http_proxy);
		cl_setenv("HTTPS_PROXY", _orig_https_proxy);

		git__free(_orig_http_proxy);
		git__free(_orig_https_proxy);
	}
}

void test_online_proxy__proxy_credentials_request(void)
{
	git_buf url = GIT_BUF_INIT;
	int called_proxy_creds = 0;

	if (!_remote_proxy_host || !_remote_proxy_user || !_remote_proxy_pass)
		cl_skip();

	cl_git_pass(git_buf_printf(&url, "%s://%s/",
		_remote_proxy_scheme ? _remote_proxy_scheme : "http",
		_remote_proxy_host));

	g_options.fetch_opts.proxy_opts.type = GIT_PROXY_SPECIFIED;
	g_options.fetch_opts.proxy_opts.url = url.ptr;
	g_options.fetch_opts.proxy_opts.credentials = proxy_cred_cb;
	g_options.fetch_opts.proxy_opts.payload = &called_proxy_creds;
	g_options.fetch_opts.proxy_opts.certificate_check = proxy_cert_cb;
	cl_git_pass(git_clone(&g_repo, REPO_URL, "./foo", &g_options));
	cl_assert(called_proxy_creds);

	git_buf_dispose(&url);
}

void test_online_proxy__proxy_credentials_in_url(void)
{
	git_buf url = GIT_BUF_INIT;
	int called_proxy_creds = 0;

	if (!_remote_proxy_host || !_remote_proxy_user || !_remote_proxy_pass)
		cl_skip();

	cl_git_pass(git_buf_printf(&url, "%s://%s:%s@%s/",
		_remote_proxy_scheme ? _remote_proxy_scheme : "http",
		_remote_proxy_user, _remote_proxy_pass, _remote_proxy_host));

	g_options.fetch_opts.proxy_opts.type = GIT_PROXY_SPECIFIED;
	g_options.fetch_opts.proxy_opts.url = url.ptr;
	g_options.fetch_opts.proxy_opts.certificate_check = proxy_cert_cb;
	g_options.fetch_opts.proxy_opts.payload = &called_proxy_creds;
	cl_git_pass(git_clone(&g_repo, REPO_URL, "./foo", &g_options));
	cl_assert(called_proxy_creds == 0);

	git_buf_dispose(&url);
}

void test_online_proxy__proxy_credentials_in_environment(void)
{
	git_buf url = GIT_BUF_INIT;

	if (!_remote_proxy_host || !_remote_proxy_user || !_remote_proxy_pass)
		cl_skip();

	_orig_http_proxy = cl_getenv("HTTP_PROXY");
	_orig_https_proxy = cl_getenv("HTTPS_PROXY");
	_orig_proxies_need_reset = 1;

	g_options.fetch_opts.proxy_opts.type = GIT_PROXY_AUTO;
	g_options.fetch_opts.proxy_opts.certificate_check = proxy_cert_cb;

	cl_git_pass(git_buf_printf(&url, "%s://%s:%s@%s/",
		_remote_proxy_scheme ? _remote_proxy_scheme : "http",
		_remote_proxy_user, _remote_proxy_pass, _remote_proxy_host));

	cl_setenv("HTTP_PROXY", url.ptr);
	cl_setenv("HTTPS_PROXY", url.ptr);

	cl_git_pass(git_clone(&g_repo, REPO_URL, "./foo", &g_options));

	git_buf_dispose(&url);
}

void test_online_proxy__proxy_auto_not_detected(void)
{
	g_options.fetch_opts.proxy_opts.type = GIT_PROXY_AUTO;

	cl_git_pass(git_clone(&g_repo, REPO_URL, "./foo", &g_options));
}

void test_online_proxy__proxy_cred_callback_after_failed_url_creds(void)
{
	git_buf url = GIT_BUF_INIT;
	int called_proxy_creds = 0;

	if (!_remote_proxy_host || !_remote_proxy_user || !_remote_proxy_pass)
		cl_skip();

	cl_git_pass(git_buf_printf(&url, "%s://invalid_user_name:INVALID_pass_WORD@%s/",
		_remote_proxy_scheme ? _remote_proxy_scheme : "http",
		_remote_proxy_host));

	g_options.fetch_opts.proxy_opts.type = GIT_PROXY_SPECIFIED;
	g_options.fetch_opts.proxy_opts.url = url.ptr;
	g_options.fetch_opts.proxy_opts.credentials = proxy_cred_cb;
	g_options.fetch_opts.proxy_opts.payload = &called_proxy_creds;
	g_options.fetch_opts.proxy_opts.certificate_check = proxy_cert_cb;
	cl_git_pass(git_clone(&g_repo, REPO_URL, "./foo", &g_options));
	cl_assert(called_proxy_creds);

	git_buf_dispose(&url);
}

