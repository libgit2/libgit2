#include "clar_libgit2.h"

#include "git2/cred_helpers.h"
#include "futils.h"
#include "remote.h"
#include "refs.h"

#define CLONE_PATH "./foo"

static struct {
	const char *normal;
	const char *empty;
	const char *nonexisting;
	const char *authenticated;
	const char *misauthenticated;
	const char *spaces;
} g_urls;

static const char *g_hostname;
static git_repository *g_repo;
static git_clone_options g_options;

static int bitbucket_creds(git_cred **cred, const char *url, const char *user, unsigned int allowed_types, void *payload)
{
	GIT_UNUSED(url); GIT_UNUSED(user); GIT_UNUSED(payload);
	if (GIT_CREDTYPE_USERNAME & allowed_types)
		return git_cred_username_new(cred, "libgit3");
	if (GIT_CREDTYPE_USERPASS_PLAINTEXT & allowed_types)
		return git_cred_userpass_plaintext_new(cred, "libgit3", "libgit3");
	return GIT_PASSTHROUGH;
}

static void initialize(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	memcpy(&g_options, &opts, sizeof(g_options));
	g_repo = NULL;
	memset(&g_urls, 0, sizeof(g_urls));
}

void test_online_clone_http__initialize_github(void)
{
	initialize();
	g_hostname = "github.com";
	g_urls.normal = "http://github.com/libgit2/TestGitRepository";
	g_urls.empty = "http://github.com/libgit2/TestEmptyRepository";
	g_urls.nonexisting = "http://github.com/libgit2/non-existent";
}

void test_online_clone_http__initialize_bitbucket(void)
{
	initialize();
	g_options.fetch_opts.callbacks.credentials = bitbucket_creds;
	g_hostname = "bitbucket.org";
	g_urls.normal = "https://bitbucket.org/libgit2/testgitrepository.git";
	g_urls.nonexisting = "https://bitbucket.org/libgit2/nonexistent.git";
	g_urls.authenticated = "https://libgit3:libgit3@bitbucket.org/libgit2/testgitrepository.git";
	g_urls.misauthenticated = "https://libgit3:wrong@bitbucket.org/libgit2/testgitrepository.git";
}

void test_online_clone_http__initialize_gitlab(void)
{
	initialize();
	g_hostname = "gitlab.com";
	g_urls.normal = "https://gitlab.com/libgit2/testgitrepository.git";
	g_urls.empty = "https://gitlab.com/libgit2/testemptyproject.git";
	g_urls.nonexisting = "https://gitlab.com/libgit2/nonexistent.git";
}

void test_online_clone_http__initialize_azure(void)
{
	initialize();
	g_hostname = "dev.azure.com";
	g_urls.normal = "https://libgit2@dev.azure.com/libgit2/test/_git/test";
	g_urls.empty = "https://libgit2@dev.azure.com/libgit2/test/_git/empty";
	g_urls.spaces = "https://libgit2@dev.azure.com/libgit2/test/_git/spaces%20in%20the%20name";
}

void test_online_clone_http__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup(CLONE_PATH);
}

void test_online_clone_http__network_full(void)
{
	git_remote *origin;

	if (!g_urls.normal)
		clar__skip();

	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));
	cl_assert(!git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_lookup(&origin, g_repo, "origin"));

	cl_assert_equal_i(GIT_REMOTE_DOWNLOAD_TAGS_AUTO, origin->download_tags);

	git_remote_free(origin);
}

void test_online_clone_http__network_bare(void)
{
	git_remote *origin;

	if (!g_urls.normal)
		clar__skip();

	g_options.bare = true;

	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));
	cl_assert(git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_lookup(&origin, g_repo, "origin"));

	git_remote_free(origin);
}

void test_online_clone_http__empty_repository(void)
{
	git_reference *head;

	if (!g_urls.empty)
		clar__skip();

	cl_git_pass(git_clone(&g_repo, g_urls.empty, CLONE_PATH, &g_options));

	cl_assert_equal_i(true, git_repository_is_empty(g_repo));
	cl_assert_equal_i(true, git_repository_head_unborn(g_repo));

	cl_git_pass(git_reference_lookup(&head, g_repo, GIT_HEAD_FILE));
	cl_assert_equal_i(GIT_REFERENCE_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));

	git_reference_free(head);
}

static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
	bool *was_called = (bool*)payload;
	GIT_UNUSED(path); GIT_UNUSED(cur); GIT_UNUSED(tot);
	(*was_called) = true;
}

static int fetch_progress(const git_indexer_progress *stats, void *payload)
{
	bool *was_called = (bool*)payload;
	GIT_UNUSED(stats);
	(*was_called) = true;
	return 0;
}

void test_online_clone_http__can_checkout_a_cloned_repo(void)
{
	git_buf path = GIT_BUF_INIT;
	git_reference *head;
	bool checkout_progress_cb_was_called = false,
		  fetch_progress_cb_was_called = false;

	if (!g_urls.normal)
		clar__skip();

	g_options.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	g_options.checkout_opts.progress_cb = &checkout_progress;
	g_options.checkout_opts.progress_payload = &checkout_progress_cb_was_called;
	g_options.fetch_opts.callbacks.transfer_progress = &fetch_progress;
	g_options.fetch_opts.callbacks.payload = &fetch_progress_cb_was_called;

	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "master.txt"));
	cl_assert_equal_i(true, git_path_isfile(git_buf_cstr(&path)));

	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_assert_equal_i(GIT_REFERENCE_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));

	cl_assert_equal_i(true, checkout_progress_cb_was_called);
	cl_assert_equal_i(true, fetch_progress_cb_was_called);

	git_reference_free(head);
	git_buf_dispose(&path);
}

static int remote_mirror_cb(git_remote **out, git_repository *repo,
			    const char *name, const char *url, void *payload)
{
	int error;
	git_remote *remote;

	GIT_UNUSED(payload);

	if ((error = git_remote_create_with_fetchspec(&remote, repo, name, url, "+refs/*:refs/*")) < 0)
		return error;

	*out = remote;
	return 0;
}

void test_online_clone_http__clone_mirror(void)
{
	bool fetch_progress_cb_was_called = false;
	git_reference *head;

	if (!g_urls.normal)
		clar__skip();

	g_options.fetch_opts.callbacks.transfer_progress = &fetch_progress;
	g_options.fetch_opts.callbacks.payload = &fetch_progress_cb_was_called;
	g_options.bare = true;
	g_options.remote_cb = remote_mirror_cb;

	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));

	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_assert_equal_i(GIT_REFERENCE_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(head));

	cl_assert_equal_i(true, fetch_progress_cb_was_called);

	git_reference_free(head);
}

static int update_tips(const char *refname, const git_oid *a, const git_oid *b, void *payload)
{
	int *callcount = (int*)payload;
	GIT_UNUSED(refname); GIT_UNUSED(a); GIT_UNUSED(b);
	*callcount = *callcount + 1;
	return 0;
}

void test_online_clone_http__custom_remote_callbacks(void)
{
	int callcount = 0;

	if (!g_urls.normal)
		clar__skip();

	g_options.fetch_opts.callbacks.update_tips = update_tips;
	g_options.fetch_opts.callbacks.payload = &callcount;

	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));
	cl_assert(callcount > 0);
}

void test_online_clone_http__custom_headers(void)
{
	char *empty_header = "";
	char *unnamed_header = "this is a header about nothing";
	char *newlines = "X-Custom: almost OK\n";
	char *conflict = "Accept: defined-by-git";
	char *ok = "X-Custom: this should be ok";

	if (!g_urls.normal)
		clar__skip();

	g_options.fetch_opts.custom_headers.count = 1;

	g_options.fetch_opts.custom_headers.strings = &empty_header;
	cl_git_fail(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));

	g_options.fetch_opts.custom_headers.strings = &unnamed_header;
	cl_git_fail(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));

	g_options.fetch_opts.custom_headers.strings = &newlines;
	cl_git_fail(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));

	g_options.fetch_opts.custom_headers.strings = &conflict;
	cl_git_fail(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));

	/* Finally, we got it right! */
	g_options.fetch_opts.custom_headers.strings = &ok;
	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));
}

static int cred_failure_cb(
	git_cred **cred,
	const char *url,
	const char *username_from_url,
	unsigned int allowed_types,
	void *data)
{
	GIT_UNUSED(cred); GIT_UNUSED(url); GIT_UNUSED(username_from_url);
	GIT_UNUSED(allowed_types); GIT_UNUSED(data);
	return -172;
}

void test_online_clone_http__cred_callback_failure_return_code_is_tunnelled(void)
{
	if (!g_urls.nonexisting)
		clar__skip();
	g_options.fetch_opts.callbacks.credentials = cred_failure_cb;

	/* Bitbucket actually returns a 404 before checking credentials */
	if (!strcmp(g_hostname, "bitbucket.org"))
		cl_git_fail_with(-1, git_clone(&g_repo, g_urls.nonexisting, CLONE_PATH, &g_options));
	else
		cl_git_fail_with(-172, git_clone(&g_repo, g_urls.nonexisting, CLONE_PATH, &g_options));
}

static int cred_count_calls_cb(git_cred **cred, const char *url, const char *user,
			       unsigned int allowed_types, void *data)
{
	size_t *counter = (size_t *) data;

	GIT_UNUSED(url); GIT_UNUSED(user); GIT_UNUSED(allowed_types);

	if (allowed_types == GIT_CREDTYPE_USERNAME)
		return git_cred_username_new(cred, "foo");

	(*counter)++;

	if (*counter == 3)
		return GIT_EUSER;

	return git_cred_userpass_plaintext_new(cred, "foo", "bar");
}

void test_online_clone_http__cred_callback_called_again_on_auth_failure(void)
{
	size_t counter = 0;

	if (!g_urls.nonexisting)
		clar__skip();

	g_options.fetch_opts.callbacks.credentials = cred_count_calls_cb;
	g_options.fetch_opts.callbacks.payload = &counter;

	/* Bitbucket actually returns a 404 before checking credentials */
	if (!strcmp(g_hostname, "bitbucket.org")) {
		cl_git_fail_with(-1, git_clone(&g_repo, g_urls.nonexisting, CLONE_PATH, &g_options));
	} else {
		cl_git_fail_with(GIT_EUSER, git_clone(&g_repo, g_urls.nonexisting, CLONE_PATH, &g_options));
		cl_assert_equal_i(3, counter);
	}
}

void test_online_clone_http__creds_in_url(void)
{
	git_cred_userpass_payload user_pass = { "libgit2", "wrong" };

	if (!g_urls.authenticated)
		clar__skip();

	g_options.fetch_opts.callbacks.credentials = git_cred_userpass;
	g_options.fetch_opts.callbacks.payload = &user_pass;

	/*
	 * Correct user and pass are in the URL; the (incorrect) creds in
	 * the `git_cred_userpass_payload` should be ignored.
	 */
	cl_git_pass(git_clone(&g_repo, g_urls.authenticated, CLONE_PATH, &g_options));
}

void test_online_clone_http__url_creds_fallback_to_explicit_creds(void)
{
	git_cred_userpass_payload user_pass = {
		"libgit2", "libgit2"
	};

	if (!g_urls.misauthenticated)
		clar__skip();

	g_options.fetch_opts.callbacks.credentials = git_cred_userpass;
	g_options.fetch_opts.callbacks.payload = &user_pass;

	/*
	 * TODO: as of March 2018, bitbucket sporadically fails with
	 * 403s instead of replying with a 401 - but only sometimes.
	 */
	cl_skip();

	/*
	 * Incorrect user and pass are in the URL; the (correct) creds in
	 * the `git_cred_userpass_payload` should be used as a fallback.
	 */
	cl_git_pass(git_clone(&g_repo, g_urls.misauthenticated, CLONE_PATH, &g_options));
}

static int cancel_at_half(const git_indexer_progress *stats, void *payload)
{
	GIT_UNUSED(payload);

	if (stats->received_objects > (stats->total_objects/2))
		return 4321;
	return 0;
}

void test_online_clone_http__can_cancel(void)
{
	if (!g_urls.normal)
		clar__skip();
	g_options.fetch_opts.callbacks.transfer_progress = cancel_at_half;
	cl_git_fail_with(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options), 4321);
}

static int fail_certificate_check(git_cert *cert, int valid, const char *host, void *payload)
{
	GIT_UNUSED(cert);
	GIT_UNUSED(valid);
	GIT_UNUSED(host);
	GIT_UNUSED(payload);
	return GIT_ECERTIFICATE;
}

void test_online_clone_http__certificate_invalid(void)
{
	if (!g_urls.normal)
		clar__skip();
	g_options.fetch_opts.callbacks.certificate_check = fail_certificate_check;
	cl_git_fail_with(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options),
		GIT_ECERTIFICATE);
}

static int succeed_certificate_check(git_cert *cert, int valid, const char *host, void *payload)
{
	GIT_UNUSED(cert);
	GIT_UNUSED(valid);
	GIT_UNUSED(payload);

	cl_assert_equal_s(g_hostname, host);

	return 0;
}

void test_online_clone_http__certificate_valid(void)
{
	if (!g_urls.normal)
		clar__skip();
	g_options.fetch_opts.callbacks.certificate_check = succeed_certificate_check;
	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));
}

void test_online_clone_http__start_with_http(void)
{
	if (!g_urls.normal)
		clar__skip();
	g_options.fetch_opts.callbacks.certificate_check = succeed_certificate_check;
	cl_git_pass(git_clone(&g_repo, g_urls.normal, CLONE_PATH, &g_options));
}

void test_online_clone_http__path_whitespace(void)
{
	if (!g_urls.spaces)
		clar__skip();
	cl_git_pass(git_clone(&g_repo, g_urls.spaces, CLONE_PATH, &g_options));
	cl_assert(git_path_exists(CLONE_PATH "/master.txt"));
}
