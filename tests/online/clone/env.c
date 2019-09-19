#include "clar_libgit2.h"

#include "git2/clone.h"
#include "git2/cred_helpers.h"
#include "git2/sys/transport.h"

#define CLONE_PATH "./foo"

static git_repository *g_repo;
static git_clone_options g_options;

static char *_remote_url = NULL;
static char *_remote_user = NULL;
static char *_remote_pass = NULL;

static char *_remote_sslnoverify = NULL;
static char *_remote_ssh_pubkey = NULL;
static char *_remote_ssh_privkey = NULL;
static char *_remote_ssh_passphrase = NULL;
static char *_remote_ssh_fingerprint = NULL;

static int certificate_check_cb(git_cert *cert, int valid, const char *host, void *payload)
{
	GIT_UNUSED(cert);
	GIT_UNUSED(host);
	GIT_UNUSED(payload);

	if (_remote_sslnoverify != NULL)
		valid = 1;


	return valid ? 0 : GIT_ECERTIFICATE;
}

void test_online_clone_env__initialize(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	memcpy(&g_options, &opts, sizeof(g_options));
	g_options.fetch_opts.callbacks.certificate_check = certificate_check_cb;

	_remote_url = cl_getenv("GITTEST_REMOTE_URL");
	_remote_user = cl_getenv("GITTEST_REMOTE_USER");
	_remote_pass = cl_getenv("GITTEST_REMOTE_PASS");
	_remote_sslnoverify = cl_getenv("GITTEST_REMOTE_SSL_NOVERIFY");
	_remote_ssh_pubkey = cl_getenv("GITTEST_REMOTE_SSH_PUBKEY");
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
	git__free(_remote_sslnoverify);
	git__free(_remote_ssh_pubkey);
	git__free(_remote_ssh_privkey);
	git__free(_remote_ssh_passphrase);
	git__free(_remote_ssh_fingerprint);
}

int cred_default(
	git_cred **cred,
	const char *url,
	const char *user_from_url,
	unsigned int allowed_types,
	void *payload)
{
	GIT_UNUSED(url);
	GIT_UNUSED(user_from_url);
	GIT_UNUSED(payload);

	if (!(allowed_types & GIT_CREDTYPE_DEFAULT))
		return 0;

	return git_cred_default_new(cred);
}

void test_online_clone_env__credentials(void)
{
	/* Remote URL environment variable must be set.
	 * User and password are optional.
	 */
	git_cred_userpass_payload user_pass = {
		_remote_user,
		_remote_pass
	};

	if (!_remote_url)
		clar__skip();

	if (cl_is_env_set("GITTEST_REMOTE_DEFAULT")) {
		g_options.fetch_opts.callbacks.credentials = cred_default;
	} else {
		g_options.fetch_opts.callbacks.credentials = git_cred_userpass;
		g_options.fetch_opts.callbacks.payload = &user_pass;
	}

	cl_git_pass(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}

static int cred_cb(git_cred **cred, const char *url, const char *user_from_url,
		   unsigned int allowed_types, void *payload)
{
	GIT_UNUSED(url); GIT_UNUSED(user_from_url); GIT_UNUSED(payload);

	if (allowed_types & GIT_CREDTYPE_USERNAME)
		return git_cred_username_new(cred, _remote_user);

	if (allowed_types & GIT_CREDTYPE_SSH_KEY)
		return git_cred_ssh_key_new(cred,
			_remote_user, _remote_ssh_pubkey,
			_remote_ssh_privkey, _remote_ssh_passphrase);

	git_error_set(GIT_ERROR_NET, "unexpected cred type");
	return -1;
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
	clar__skip();
#endif
	if (!_remote_url || !_remote_user || strncmp(_remote_url, "ssh://", 5) != 0)
		clar__skip();

	g_options.remote_cb = custom_remote_ssh_with_paths;
	g_options.fetch_opts.callbacks.transport = git_transport_ssh_with_paths;
	g_options.fetch_opts.callbacks.credentials = cred_cb;
	g_options.fetch_opts.callbacks.payload = &arr;
	g_options.fetch_opts.callbacks.certificate_check = NULL;

	cl_git_fail(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));

	arr.strings = good_paths;
	cl_git_pass(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}

static int ssh_certificate_check(git_cert *cert, int valid, const char *host, void *payload)
{
	git_cert_hostkey *key;
	git_oid expected = {{0}}, actual = {{0}};

	GIT_UNUSED(valid);
	GIT_UNUSED(payload);

	cl_assert(_remote_ssh_fingerprint);

	cl_git_pass(git_oid_fromstrp(&expected, _remote_ssh_fingerprint));
	cl_assert_equal_i(GIT_CERT_HOSTKEY_LIBSSH2, cert->cert_type);
	key = (git_cert_hostkey *) cert;

	/*
	 * We need to figure out how long our input was to check for
	 * the type. Here we abuse the fact that both hashes fit into
	 * our git_oid type.
	 */
	if (strlen(_remote_ssh_fingerprint) == 32 && key->type & GIT_CERT_SSH_MD5) {
		memcpy(&actual.id, key->hash_md5, 16);
	} else 	if (strlen(_remote_ssh_fingerprint) == 40 && key->type & GIT_CERT_SSH_SHA1) {
		memcpy(&actual, key->hash_sha1, 20);
	} else {
		cl_fail("Cannot find a usable SSH hash");
	}

	cl_assert(!memcmp(&expected, &actual, 20));

	cl_assert_equal_s("localhost", host);

	return GIT_EUSER;
}

void test_online_clone_env__ssh_cert(void)
{
	g_options.fetch_opts.callbacks.certificate_check = ssh_certificate_check;

	if (!_remote_ssh_fingerprint)
		cl_skip();

	cl_git_fail_with(GIT_EUSER, git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}

static char *read_key_file(const char *path)
{
	FILE *f;
	char *buf;
	long key_length;

	if (!path || !*path)
		return NULL;

	cl_assert((f = fopen(path, "r")) != NULL);
	cl_assert(fseek(f, 0, SEEK_END) != -1);
	cl_assert((key_length = ftell(f)) != -1);
	cl_assert(fseek(f, 0, SEEK_SET) != -1);
	cl_assert((buf = malloc(key_length)) != NULL);
	cl_assert(fread(buf, key_length, 1, f) == 1);
	fclose(f);

	return buf;
}

static int ssh_memory_cred_cb(git_cred **cred, const char *url, const char *user_from_url,
		   unsigned int allowed_types, void *payload)
{
	GIT_UNUSED(url); GIT_UNUSED(user_from_url); GIT_UNUSED(payload);

	if (allowed_types & GIT_CREDTYPE_USERNAME)
		return git_cred_username_new(cred, _remote_user);

	if (allowed_types & GIT_CREDTYPE_SSH_KEY)
	{
		char *pubkey = read_key_file(_remote_ssh_pubkey);
		char *privkey = read_key_file(_remote_ssh_privkey);

		int ret = git_cred_ssh_key_memory_new(cred, _remote_user, pubkey, privkey, _remote_ssh_passphrase);

		if (privkey)
			free(privkey);
		if (pubkey)
			free(pubkey);
		return ret;
	}

	git_error_set(GIT_ERROR_NET, "unexpected cred type");
	return -1;
}

void test_online_clone_env__ssh_memory_auth(void)
{
#ifndef GIT_SSH_MEMORY_CREDENTIALS
	clar__skip();
#endif
	if (!_remote_url || !_remote_user || !_remote_ssh_privkey || strncmp(_remote_url, "ssh://", 5) != 0)
		clar__skip();

	g_options.fetch_opts.callbacks.credentials = ssh_memory_cred_cb;

	cl_git_pass(git_clone(&g_repo, _remote_url, CLONE_PATH, &g_options));
}
