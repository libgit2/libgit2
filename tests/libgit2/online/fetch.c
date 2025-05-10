#include "clar_libgit2.h"
#include "futils.h"

static git_repository *_repo;
static int counter;

static char *_remote_proxy_scheme = NULL;
static char *_remote_proxy_host = NULL;
static char *_remote_proxy_user = NULL;
static char *_remote_proxy_pass = NULL;
static char *_remote_redirect_initial = NULL;
static char *_remote_redirect_subsequent = NULL;

void test_online_fetch__initialize(void)
{
	cl_git_pass(git_repository_init(&_repo, "./fetch", 0));

	_remote_proxy_scheme = cl_getenv("GITTEST_REMOTE_PROXY_SCHEME");
	_remote_proxy_host = cl_getenv("GITTEST_REMOTE_PROXY_HOST");
	_remote_proxy_user = cl_getenv("GITTEST_REMOTE_PROXY_USER");
	_remote_proxy_pass = cl_getenv("GITTEST_REMOTE_PROXY_PASS");
	_remote_redirect_initial = cl_getenv("GITTEST_REMOTE_REDIRECT_INITIAL");
	_remote_redirect_subsequent = cl_getenv("GITTEST_REMOTE_REDIRECT_SUBSEQUENT");
}

void test_online_fetch__cleanup(void)
{
	git_repository_free(_repo);
	_repo = NULL;

	cl_fixture_cleanup("./fetch");
	cl_fixture_cleanup("./redirected");

	git__free(_remote_proxy_scheme);
	git__free(_remote_proxy_host);
	git__free(_remote_proxy_user);
	git__free(_remote_proxy_pass);
	git__free(_remote_redirect_initial);
	git__free(_remote_redirect_subsequent);
}

static int update_refs(const char *refname, const git_oid *a, const git_oid *b, git_refspec *spec, void *data)
{
	GIT_UNUSED(refname);
	GIT_UNUSED(a);
	GIT_UNUSED(b);
	GIT_UNUSED(spec);
	GIT_UNUSED(data);

	++counter;

	return 0;
}

static int progress(const git_indexer_progress *stats, void *payload)
{
	size_t *bytes_received = (size_t *)payload;
	*bytes_received = stats->received_bytes;
	return 0;
}

static void do_fetch(const char *url, git_remote_autotag_option_t flag, int n)
{
	git_remote *remote;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	size_t bytes_received = 0;

	options.callbacks.transfer_progress = progress;
	options.callbacks.update_refs = update_refs;
	options.callbacks.payload = &bytes_received;
	options.download_tags = flag;
	counter = 0;

	cl_git_pass(git_remote_create(&remote, _repo, "test", url));
	cl_git_pass(git_remote_fetch(remote, NULL, &options, NULL));
	cl_assert_equal_i(counter, n);
	cl_assert(bytes_received > 0);

	git_remote_free(remote);
}

void test_online_fetch__default_http(void)
{
	do_fetch("http://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_AUTO, 6);
}

void test_online_fetch__default_https(void)
{
	do_fetch("https://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_AUTO, 6);
}

void test_online_fetch__no_tags_git(void)
{
	do_fetch("https://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_NONE, 3);
}

void test_online_fetch__no_tags_http(void)
{
	do_fetch("http://github.com/libgit2/TestGitRepository.git", GIT_REMOTE_DOWNLOAD_TAGS_NONE, 3);
}

void test_online_fetch__fetch_twice(void)
{
	git_remote *remote;
	cl_git_pass(git_remote_create(&remote, _repo, "test", "https://github.com/libgit2/TestGitRepository.git"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	cl_git_pass(git_remote_download(remote, NULL, NULL));
		git_remote_disconnect(remote, NULL);

	git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL);
	cl_git_pass(git_remote_download(remote, NULL, NULL));
	git_remote_disconnect(remote, NULL);

	git_remote_free(remote);
}

void test_online_fetch__fetch_with_empty_http_proxy(void)
{
	git_remote *remote;
	git_config *config;
	git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;

	opts.proxy_opts.type = GIT_PROXY_AUTO;

	cl_git_pass(git_repository_config(&config, _repo));
	cl_git_pass(git_config_set_string(config, "http.proxy", ""));

	cl_git_pass(git_remote_create(&remote, _repo, "test",
		"https://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_remote_fetch(remote, NULL, &opts, NULL));

	git_remote_disconnect(remote);
	git_remote_free(remote);
	git_config_free(config);
}

static int transferProgressCallback(const git_indexer_progress *stats, void *payload)
{
	bool *invoked = (bool *)payload;

	GIT_UNUSED(stats);
	*invoked = true;
	return 0;
}

void test_online_fetch__doesnt_retrieve_a_pack_when_the_repository_is_up_to_date(void)
{
	git_repository *_repository;
	bool invoked = false;
	git_remote *remote;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.bare = true;

	counter = 0;

	cl_git_pass(git_clone(&_repository, "https://github.com/libgit2/TestGitRepository.git",
				"./fetch/lg2", &opts));
	git_repository_free(_repository);

	cl_git_pass(git_repository_open(&_repository, "./fetch/lg2"));

	cl_git_pass(git_remote_lookup(&remote, _repository, "origin"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));

	cl_assert_equal_i(false, invoked);

	options.callbacks.transfer_progress = &transferProgressCallback;
	options.callbacks.payload = &invoked;
	options.callbacks.update_refs = update_refs;
	cl_git_pass(git_remote_download(remote, NULL, &options));

	cl_assert_equal_i(false, invoked);

	cl_git_pass(git_remote_update_tips(remote, &options.callbacks, GIT_REMOTE_UPDATE_FETCHHEAD, options.download_tags, NULL));
	cl_assert_equal_i(0, counter);

	git_remote_disconnect(remote, NULL);

	git_remote_free(remote);
	git_repository_free(_repository);
}

void test_online_fetch__report_unchanged_tips(void)
{
	git_repository *_repository;
	bool invoked = false;
	git_remote *remote;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	opts.bare = true;

	counter = 0;

	cl_git_pass(git_clone(&_repository, "https://github.com/libgit2/TestGitRepository.git",
				"./fetch/lg2", &opts));
	git_repository_free(_repository);

	cl_git_pass(git_repository_open(&_repository, "./fetch/lg2"));

	cl_git_pass(git_remote_lookup(&remote, _repository, "origin"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));

	cl_assert_equal_i(false, invoked);

	options.callbacks.transfer_progress = &transferProgressCallback;
	options.callbacks.payload = &invoked;
	options.callbacks.update_refs = update_refs;
	cl_git_pass(git_remote_download(remote, NULL, &options));

	cl_assert_equal_i(false, invoked);

	cl_git_pass(git_remote_update_tips(remote, &options.callbacks, GIT_REMOTE_UPDATE_REPORT_UNCHANGED, options.download_tags, NULL));
	cl_assert(counter > 0);

	git_remote_disconnect(remote);

	git_remote_free(remote);
	git_repository_free(_repository);
}

static int cancel_at_half(const git_indexer_progress *stats, void *payload)
{
	GIT_UNUSED(payload);

	if (stats->received_objects > (stats->total_objects/2))
		return -4321;
	return 0;
}

void test_online_fetch__can_cancel(void)
{
	git_remote *remote;
	size_t bytes_received = 0;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;

	cl_git_pass(git_remote_create(&remote, _repo, "test",
				"http://github.com/libgit2/TestGitRepository.git"));

	options.callbacks.transfer_progress = cancel_at_half;
	options.callbacks.payload = &bytes_received;

	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	cl_git_fail_with(git_remote_download(remote, NULL, &options), -4321);
	git_remote_disconnect(remote, NULL);
	git_remote_free(remote);
}

void test_online_fetch__ls_disconnected(void)
{
	const git_remote_head **refs;
	size_t refs_len_before, refs_len_after;
	git_remote *remote;

	cl_git_pass(git_remote_create(&remote, _repo, "test",
				"http://github.com/libgit2/TestGitRepository.git"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	cl_git_pass(git_remote_ls(&refs, &refs_len_before, remote));
	git_remote_disconnect(remote, NULL);
	cl_git_pass(git_remote_ls(&refs, &refs_len_after, remote));

	cl_assert_equal_i(refs_len_before, refs_len_after);

	git_remote_free(remote);
}

void test_online_fetch__remote_symrefs(void)
{
	const git_remote_head **refs;
	size_t refs_len;
	git_remote *remote;

	cl_git_pass(git_remote_create(&remote, _repo, "test",
				"http://github.com/libgit2/TestGitRepository.git"));
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	git_remote_disconnect(remote, NULL);
	cl_git_pass(git_remote_ls(&refs, &refs_len, remote));

	cl_assert_equal_s("HEAD", refs[0]->name);
	cl_assert_equal_s("refs/heads/master", refs[0]->symref_target);

	git_remote_free(remote);
}

void test_online_fetch__twice(void)
{
	git_remote *remote;

	cl_git_pass(git_remote_create(&remote, _repo, "test", "http://github.com/libgit2/TestGitRepository.git"));
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));

	git_remote_free(remote);
}

void test_online_fetch__proxy(void)
{
    git_remote *remote;
    git_str url = GIT_STR_INIT;
    git_fetch_options fetch_opts;

    if (!_remote_proxy_host || !_remote_proxy_user || !_remote_proxy_pass)
        cl_skip();

    cl_git_pass(git_str_printf(&url, "%s://%s:%s@%s/",
        _remote_proxy_scheme ? _remote_proxy_scheme : "http",
        _remote_proxy_user, _remote_proxy_pass, _remote_proxy_host));

    cl_git_pass(git_fetch_options_init(&fetch_opts, GIT_FETCH_OPTIONS_VERSION));
    fetch_opts.proxy_opts.type = GIT_PROXY_SPECIFIED;
    fetch_opts.proxy_opts.url = url.ptr;

    cl_git_pass(git_remote_create(&remote, _repo, "test", "https://github.com/libgit2/TestGitRepository.git"));
    cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, &fetch_opts.proxy_opts, NULL));
    cl_git_pass(git_remote_fetch(remote, NULL, &fetch_opts, NULL));

    git_remote_free(remote);
    git_str_dispose(&url);
}

static int do_redirected_fetch(const char *url, const char *name, const char *config)
{
	git_repository *repo;
	git_remote *remote;
	int error;

	cl_git_pass(git_repository_init(&repo, "./redirected", 0));
	cl_fixture_cleanup(name);

	if (config)
		cl_repo_set_string(repo, "http.followRedirects", config);

	cl_git_pass(git_remote_create(&remote, repo, name, url));
	error = git_remote_fetch(remote, NULL, NULL, NULL);

	git_remote_free(remote);
	git_repository_free(repo);

	cl_fixture_cleanup("./redirected");

	return error;
}

void test_online_fetch__redirect_config(void)
{
	if (!_remote_redirect_initial || !_remote_redirect_subsequent)
		cl_skip();

	/* config defaults */
	cl_git_pass(do_redirected_fetch(_remote_redirect_initial, "initial", NULL));
	cl_git_fail(do_redirected_fetch(_remote_redirect_subsequent, "subsequent", NULL));

	/* redirect=initial */
	cl_git_pass(do_redirected_fetch(_remote_redirect_initial, "initial", "initial"));
	cl_git_fail(do_redirected_fetch(_remote_redirect_subsequent, "subsequent", "initial"));

	/* redirect=false */
	cl_git_fail(do_redirected_fetch(_remote_redirect_initial, "initial", "false"));
	cl_git_fail(do_redirected_fetch(_remote_redirect_subsequent, "subsequent", "false"));
}

void test_online_fetch__reachable_commit(void)
{
	git_remote *remote;
	git_strarray refspecs;
	git_object *obj;
	git_oid expected_id;
	git_str fetchhead = GIT_STR_INIT;
	char *refspec = "+2c349335b7f797072cf729c4f3bb0914ecb6dec9:refs/success";

	refspecs.strings = &refspec;
	refspecs.count = 1;

	git_oid_from_string(&expected_id, "2c349335b7f797072cf729c4f3bb0914ecb6dec9", GIT_OID_SHA1);

	cl_git_pass(git_remote_create(&remote, _repo, "test",
		"https://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_remote_fetch(remote, &refspecs, NULL, NULL));

	cl_git_pass(git_revparse_single(&obj, _repo, "refs/success"));
	cl_assert_equal_oid(&expected_id, git_object_id(obj));

	cl_git_pass(git_futils_readbuffer(&fetchhead, "./fetch/.git/FETCH_HEAD"));
	cl_assert_equal_s(fetchhead.ptr,
		"2c349335b7f797072cf729c4f3bb0914ecb6dec9\t\t'2c349335b7f797072cf729c4f3bb0914ecb6dec9' of https://github.com/libgit2/TestGitRepository\n");

	git_str_dispose(&fetchhead);
	git_object_free(obj);
	git_remote_free(remote);
}

void test_online_fetch__reachable_commit_without_destination(void)
{
	git_remote *remote;
	git_strarray refspecs;
	git_object *obj;
	git_oid expected_id;
	git_str fetchhead = GIT_STR_INIT;
	char *refspec = "2c349335b7f797072cf729c4f3bb0914ecb6dec9";

	refspecs.strings = &refspec;
	refspecs.count = 1;

	git_oid_from_string(&expected_id, "2c349335b7f797072cf729c4f3bb0914ecb6dec9", GIT_OID_SHA1);

	cl_git_pass(git_remote_create(&remote, _repo, "test",
		"https://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_remote_fetch(remote, &refspecs, NULL, NULL));

	cl_git_fail_with(GIT_ENOTFOUND, git_revparse_single(&obj, _repo, "refs/success"));

	cl_git_pass(git_futils_readbuffer(&fetchhead, "./fetch/.git/FETCH_HEAD"));
	cl_assert_equal_s(fetchhead.ptr,
		"2c349335b7f797072cf729c4f3bb0914ecb6dec9\t\t'2c349335b7f797072cf729c4f3bb0914ecb6dec9' of https://github.com/libgit2/TestGitRepository\n");

	git_str_dispose(&fetchhead);
	git_object_free(obj);
	git_remote_free(remote);
}
