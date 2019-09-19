#include "clar_libgit2.h"

#include "git2/cred_helpers.h"
#include "futils.h"
#include "remote.h"
#include "refs.h"

#define GH_REPO_URL "http://github.com/libgit2/TestGitRepository"
#define GH_REPO_EMPTY_URL "http://github.com/libgit2/TestEmptyRepository"
#define GH_REPO_NONEXISTENT_URL "http://github.com/libgit2/non-existent"
#define BB_REPO_URL "https://libgit3@bitbucket.org/libgit2/testgitrepository.git"
#define BB_REPO_URL_WITH_PASS "https://libgit3:libgit3@bitbucket.org/libgit2/testgitrepository.git"
#define BB_REPO_URL_WITH_WRONG_PASS "https://libgit3:wrong@bitbucket.org/libgit2/testgitrepository.git"
#define AZURE_REPO_SPACES_URL "https://libgit2@dev.azure.com/libgit2/test/_git/spaces%20in%20the%20name"

#define CLONE_PATH "./foo"

static git_repository *g_repo;
static git_clone_options g_options;

void test_online_clone_http__initialize(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	memcpy(&g_options, &opts, sizeof(g_options));
	g_repo = NULL;
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

	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));
	cl_assert(!git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_lookup(&origin, g_repo, "origin"));

	cl_assert_equal_i(GIT_REMOTE_DOWNLOAD_TAGS_AUTO, origin->download_tags);

	git_remote_free(origin);
}

void test_online_clone_http__network_bare(void)
{
	git_remote *origin;

	g_options.bare = true;

	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));
	cl_assert(git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_lookup(&origin, g_repo, "origin"));

	git_remote_free(origin);
}

void test_online_clone_http__empty_repository(void)
{
	git_reference *head;

	cl_git_pass(git_clone(&g_repo, GH_REPO_EMPTY_URL, CLONE_PATH, &g_options));

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

	g_options.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	g_options.checkout_opts.progress_cb = &checkout_progress;
	g_options.checkout_opts.progress_payload = &checkout_progress_cb_was_called;
	g_options.fetch_opts.callbacks.transfer_progress = &fetch_progress;
	g_options.fetch_opts.callbacks.payload = &fetch_progress_cb_was_called;

	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));

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
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	bool fetch_progress_cb_was_called = false;
	git_reference *head;

	opts.fetch_opts.callbacks.transfer_progress = &fetch_progress;
	opts.fetch_opts.callbacks.payload = &fetch_progress_cb_was_called;
	opts.bare = true;
	opts.remote_cb = remote_mirror_cb;

	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &opts));

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

	g_options.fetch_opts.callbacks.update_tips = update_tips;
	g_options.fetch_opts.callbacks.payload = &callcount;

	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));
	cl_assert(callcount > 0);
}

void test_online_clone_http__custom_headers(void)
{
	char *empty_header = "";
	char *unnamed_header = "this is a header about nothing";
	char *newlines = "X-Custom: almost OK\n";
	char *conflict = "Accept: defined-by-git";
	char *ok = "X-Custom: this should be ok";

	g_options.fetch_opts.custom_headers.count = 1;

	g_options.fetch_opts.custom_headers.strings = &empty_header;
	cl_git_fail(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));

	g_options.fetch_opts.custom_headers.strings = &unnamed_header;
	cl_git_fail(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));

	g_options.fetch_opts.custom_headers.strings = &newlines;
	cl_git_fail(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));

	g_options.fetch_opts.custom_headers.strings = &conflict;
	cl_git_fail(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));

	/* Finally, we got it right! */
	g_options.fetch_opts.custom_headers.strings = &ok;
	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));
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
	g_options.fetch_opts.callbacks.credentials = cred_failure_cb;
	cl_git_fail_with(-172, git_clone(&g_repo, GH_REPO_NONEXISTENT_URL, CLONE_PATH, &g_options));
}

static int cred_count_calls_cb(git_cred **cred, const char *url, const char *user,
			       unsigned int allowed_types, void *data)
{
	size_t *counter = (size_t *) data;

	GIT_UNUSED(url); GIT_UNUSED(user); GIT_UNUSED(allowed_types);

	if (allowed_types == GIT_CREDTYPE_USERNAME)
		return git_cred_username_new(cred, CLONE_PATH);

	(*counter)++;

	if (*counter == 3)
		return GIT_EUSER;

	return git_cred_userpass_plaintext_new(cred, "foo", "bar");
}

void test_online_clone_http__cred_callback_called_again_on_auth_failure(void)
{
	size_t counter = 0;

	g_options.fetch_opts.callbacks.credentials = cred_count_calls_cb;
	g_options.fetch_opts.callbacks.payload = &counter;

	cl_git_fail_with(GIT_EUSER, git_clone(&g_repo, GH_REPO_NONEXISTENT_URL, CLONE_PATH, &g_options));
	cl_assert_equal_i(3, counter);
}

void test_online_clone_http__bitbucket_style(void)
{
	git_cred_userpass_payload user_pass = {
		"libgit3", "libgit3"
	};

	g_options.fetch_opts.callbacks.credentials = git_cred_userpass;
	g_options.fetch_opts.callbacks.payload = &user_pass;

	cl_git_pass(git_clone(&g_repo, BB_REPO_URL, CLONE_PATH, &g_options));
}

void test_online_clone_http__bitbucket_uses_creds_in_url(void)
{
	git_cred_userpass_payload user_pass = {
		"libgit2", "wrong"
	};

	g_options.fetch_opts.callbacks.credentials = git_cred_userpass;
	g_options.fetch_opts.callbacks.payload = &user_pass;

	/*
	 * Correct user and pass are in the URL; the (incorrect) creds in
	 * the `git_cred_userpass_payload` should be ignored.
	 */
	cl_git_pass(git_clone(&g_repo, BB_REPO_URL_WITH_PASS, CLONE_PATH, &g_options));
}

void test_online_clone_http__bitbucket_falls_back_to_specified_creds(void)
{
	git_cred_userpass_payload user_pass = {
		"libgit2", "libgit2"
	};

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
	cl_git_pass(git_clone(&g_repo, BB_REPO_URL_WITH_WRONG_PASS, CLONE_PATH, &g_options));
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
	g_options.fetch_opts.callbacks.transfer_progress = cancel_at_half;
	cl_git_fail_with(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options), 4321);
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
	g_options.fetch_opts.callbacks.certificate_check = fail_certificate_check;
	cl_git_fail_with(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options),
		GIT_ECERTIFICATE);
}

static int succeed_certificate_check(git_cert *cert, int valid, const char *host, void *payload)
{
	GIT_UNUSED(cert);
	GIT_UNUSED(valid);
	GIT_UNUSED(payload);

	cl_assert_equal_s("github.com", host);

	return 0;
}

void test_online_clone_http__certificate_valid(void)
{
	g_options.fetch_opts.callbacks.certificate_check = succeed_certificate_check;
	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));
}

void test_online_clone_http__start_with_http(void)
{
	g_options.fetch_opts.callbacks.certificate_check = succeed_certificate_check;
	cl_git_pass(git_clone(&g_repo, GH_REPO_URL, CLONE_PATH, &g_options));
}

void test_online_clone_http__path_whitespace(void)
{
	cl_git_pass(git_clone(&g_repo, AZURE_REPO_SPACES_URL, CLONE_PATH, &g_options));
	cl_assert(git_path_exists(CLONE_PATH "/master.txt"));
}
