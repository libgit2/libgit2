#include "clar.h"
#include "clar_libgit2.h"

#include "path.h"
#include "git2/clone.h"
#include "git2/cred_helpers.h"
#include "remote.h"
#include "futils.h"
#include "refs.h"
#include "str.h"
#include "streams/openssl.h"

#if (GIT_OPENSSL && !GIT_OPENSSL_DYNAMIC)
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/x509v3.h>
#endif

/*
 * Certificates for https://test.libgit2.org/ are in the `certs` folder.
 */
#define CUSTOM_CERT_DIR "certs"

#define CUSTOM_CERT_ONE_URL "https://test.libgit2.org:1443/anonymous/test.git"
#define CUSTOM_CERT_ONE_PATH "one"

#define CUSTOM_CERT_TWO_URL "https://test.libgit2.org:2443/anonymous/test.git"
#define CUSTOM_CERT_TWO_FILE "two.pem"

#define CUSTOM_CERT_THREE_URL "https://test.libgit2.org:3443/anonymous/test.git"
#define CUSTOM_CERT_THREE_FILE "three.pem.raw"

#if (GIT_OPENSSL || GIT_MBEDTLS)
static git_repository *g_repo;
#endif

void test_online_customcert__initialize(void)
{
#if (GIT_OPENSSL || GIT_MBEDTLS)
	git_str path = GIT_STR_INIT, file = GIT_STR_INIT;
	char cwd[GIT_PATH_MAX];

	g_repo = NULL;

	cl_fixture_sandbox(CUSTOM_CERT_DIR);

	cl_must_pass(p_getcwd(cwd, GIT_PATH_MAX));
	cl_git_pass(git_str_join_n(&path, '/', 3, cwd, CUSTOM_CERT_DIR, CUSTOM_CERT_ONE_PATH));
	cl_git_pass(git_str_join_n(&file, '/', 3, cwd, CUSTOM_CERT_DIR, CUSTOM_CERT_TWO_FILE));

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS,
	                             file.ptr, path.ptr));

	git_str_dispose(&file);
	git_str_dispose(&path);
#endif
}

void test_online_customcert__cleanup(void)
{
#if (GIT_OPENSSL || GIT_MBEDTLS)
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	cl_fixture_cleanup("./cloned");
	cl_fixture_cleanup(CUSTOM_CERT_DIR);
#endif

#ifdef GIT_OPENSSL
	git_openssl__reset_context();
#endif
}

void test_online_customcert__file(void)
{
#if (GIT_OPENSSL || GIT_MBEDTLS)
	cl_git_pass(git_clone(&g_repo, CUSTOM_CERT_ONE_URL, "./cloned", NULL));
	cl_assert(git_fs_path_exists("./cloned/master.txt"));
#endif
}

void test_online_customcert__path(void)
{
#if (GIT_OPENSSL || GIT_MBEDTLS)
	cl_git_pass(git_clone(&g_repo, CUSTOM_CERT_TWO_URL, "./cloned", NULL));
	cl_assert(git_fs_path_exists("./cloned/master.txt"));
#endif
}

void test_online_customcert__raw_x509(void)
{
#if (GIT_OPENSSL && !GIT_OPENSSL_DYNAMIC)
	X509* x509_cert = NULL;
	char cwd[GIT_PATH_MAX];
	git_str raw_file = GIT_STR_INIT,
		raw_file_data = GIT_STR_INIT,
		raw_cert = GIT_STR_INIT;
	const unsigned char *raw_cert_bytes = NULL;

	cl_must_pass(p_getcwd(cwd, GIT_PATH_MAX));

	cl_git_pass(git_str_join_n(&raw_file, '/', 3, cwd, CUSTOM_CERT_DIR, CUSTOM_CERT_THREE_FILE));

	cl_git_pass(git_futils_readbuffer(&raw_file_data, git_str_cstr(&raw_file)));
	cl_git_pass(git_str_decode_base64(&raw_cert, git_str_cstr(&raw_file_data), git_str_len(&raw_file_data)));

	raw_cert_bytes = (const unsigned char *)git_str_cstr(&raw_cert);
	x509_cert = d2i_X509(NULL, &raw_cert_bytes, git_str_len(&raw_cert));
	cl_git_pass(git_libgit2_opts(GIT_OPT_ADD_SSL_X509_CERT, x509_cert));
	X509_free(x509_cert);

	cl_git_pass(git_clone(&g_repo, CUSTOM_CERT_THREE_URL, "./cloned", NULL));
	cl_assert(git_fs_path_exists("./cloned/master.txt"));

	git_str_dispose(&raw_cert);
	git_str_dispose(&raw_file_data);
	git_str_dispose(&raw_file);
#endif
}
