#include "clar_libgit2.h"

#include "git2/clone.h"
#include "git2/cred_helpers.h"
#include "git2/sys/transport.h"
#include "futils.h"

#define CLONE_PATH "./foo"

static git_repository *g_repo;
static git_clone_options g_options;

static char *_remote_url = NULL;
static char *_remote_user = NULL;
static char *_remote_pass = NULL;

static char *_remote_ssh_privkey = NULL;
static char *_remote_ssh_passphrase = NULL;
static char *_remote_ssh_fingerprint = NULL;

static struct {
	unsigned int allowed_types;
} credentials_cb_opts;

static int certificate_check_cb(git_cert *cert, int valid, const char *host, void *payload)
{
	GIT_UNUSED(host);
	GIT_UNUSED(valid);
	GIT_UNUSED(payload);

	if (_remote_ssh_fingerprint && cert->cert_type == GIT_CERT_HOSTKEY_LIBSSH2) {
		git_cert_hostkey *key = (git_cert_hostkey *) cert;
		git_oid expected;

		cl_git_pass(git_oid_fromstrp(&expected, _remote_ssh_fingerprint));

		/*
		 * We need to figure out how long our input was to check for
		 * the type. Here we abuse the fact that both hashes fit into
		 * our git_oid type.
		 */
		if (strlen(_remote_ssh_fingerprint) == 32 && key->type & GIT_CERT_SSH_MD5) {
			return (memcmp(expected.id, key->hash_md5, 16) == 0) ? 0 : -1;
		} else 	if (strlen(_remote_ssh_fingerprint) == 40 && key->type & GIT_CERT_SSH_SHA1) {
			return (memcmp(expected.id, key->hash_sha1, 20) == 0) ? 0 : -1;
		}
	}

	return -1;
}

static int credentials_cb(git_cred **cred, const char *url, const char *user_from_url,
		   unsigned int allowed_types, void *_payload)
{
	GIT_UNUSED(url);
	GIT_UNUSED(user_from_url);
	GIT_UNUSED(_payload);

	if (credentials_cb_opts.allowed_types)
		allowed_types &= credentials_cb_opts.allowed_types;

	if (allowed_types & GIT_CREDTYPE_USERNAME && _remote_user)
		return git_cred_username_new(cred, _remote_user);

	if (allowed_types & GIT_CREDTYPE_USERPASS_PLAINTEXT && _remote_user && _remote_pass)
		return git_cred_userpass_plaintext_new(cred, _remote_user, _remote_pass);

	if (allowed_types & GIT_CREDTYPE_SSH_KEY && _remote_ssh_privkey) {
		return git_cred_ssh_key_new(cred, _remote_user, NULL,
					    _remote_ssh_privkey, _remote_ssh_passphrase);
	}

	if (allowed_types & GIT_CREDTYPE_SSH_MEMORY && _remote_ssh_privkey) {
		git_buf privkey = GIT_BUF_INIT;
		int error;

		cl_git_pass(git_futils_readbuffer(&privkey, _remote_ssh_privkey));
		error = git_cred_ssh_key_memory_new(cred, _remote_user, NULL,
						    privkey.ptr, _remote_ssh_passphrase);
		git_buf_dispose(&privkey);

		return error;
	}

	git_error_set(GIT_ERROR_NET, "unexpected cred type");
	return -1;
}

void test_online_clone_env__initialize(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	memset(&credentials_cb_opts, 0, sizeof(credentials_cb_opts));

	memcpy(&g_options, &opts, sizeof(g_options));
	g_options.fetch_opts.callbacks.certificate_check = certificate_check_cb;
	g_options.fetch_opts.callbacks.credentials = credentials_cb;

	_remote_url = cl_getenv("GITTEST_REMOTE_URL");
	_remote_user = cl_getenv("GITTEST_REMOTE_USER");
	_remote_pass = cl_getenv("GITTEST_REMOTE_PASS");
	_remote_ssh_privkey = cl_getenv("GITTEST_REMOTE_SSH_KEY");
	_remote_ssh_passphrase = cl_getenv("GITTEST_REMOTE_SSH_PASSPHRASE");
	_remote_ssh_fingerprint = cl_getenv("GITTEST_REMOTE_SSH_FINGERPRINT");
}

void test_online_clone_env__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup(CLONE_PATH);

	git__free(_remote_url);
	git__free(_remote_user);
	git__free(_remote_pass);
	git__free(_remote_ssh_privkey);
	git__free(_remote_ssh_passphrase);
	git__free(_remote_ssh_fingerprint);
}

void test_online_clone_env__userpass_authentication(void)
{
	if (!_remote_url)
		clar__skip();

	credentials_cb_opts.allowed_types = GIT_CREDTYPE_USERPASS_PLAINTEXT|GIT_CREDTYPE_USERNAME;
	cl_git_pass(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}

static int custom_remote_ssh_with_paths(
	git_remote **out,
	git_repository *repo,
	const char *name,
	const char *url,
	void *payload)
{
	int error;

	GIT_UNUSED(payload);

	if ((error = git_remote_create(out, repo, name, url)) < 0)
		return error;

	return 0;
}

void test_online_clone_env__ssh_with_paths(void)
{
	char *bad_paths[] = {
		"/bin/yes",
		"/bin/false",
	};
	char *good_paths[] = {
		"/usr/bin/git-upload-pack",
		"/usr/bin/git-receive-pack",
	};
	git_strarray arr = {
		bad_paths,
		2,
	};

#ifndef GIT_SSH
	if (!_remote_url || !_remote_user || strncmp(_remote_url, "ssh://", 5) != 0)
#endif
		clar__skip();

	g_options.remote_cb = custom_remote_ssh_with_paths;
	g_options.fetch_opts.callbacks.transport = git_transport_ssh_with_paths;
	g_options.fetch_opts.callbacks.payload = &arr;
	g_options.fetch_opts.callbacks.certificate_check = NULL;

	cl_git_fail(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));

	arr.strings = good_paths;
	cl_git_pass(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}

void test_online_clone_env__ssh_key_authentication(void)
{
#ifndef GIT_SSH
	if (!_remote_url || !_remote_user || !_remote_ssh_privkey || strncmp(_remote_url, "ssh://", 5) != 0)
#endif
		clar__skip();

	credentials_cb_opts.allowed_types = GIT_CREDTYPE_SSH_KEY|GIT_CREDTYPE_USERNAME;
	cl_git_pass(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}

void test_online_clone_env__ssh_inmemory_authentication(void)
{
#ifndef GIT_SSH_MEMORY_CREDENTIALS
	if (!_remote_url || !_remote_user || !_remote_ssh_privkey || strncmp(_remote_url, "ssh://", 5) != 0)
#endif
		clar__skip();

	credentials_cb_opts.allowed_types = GIT_CREDTYPE_SSH_MEMORY|GIT_CREDTYPE_USERNAME;
	cl_git_pass(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}
