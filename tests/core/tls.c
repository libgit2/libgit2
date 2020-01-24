#include "clar_libgit2.h"
#include "streams/tls.h"

void test_core_tls__one_cipher(void)
{
	const char *name = NULL;
	size_t len = 0;

	const char *ciphers = "MY_CIPHER";

	cl_git_pass(git_tls_ciphers_foreach(&name, &len, &ciphers));
	cl_assert_equal_i(len, strlen("MY_CIPHER"));
	cl_assert_equal_strn(name, "MY_CIPHER", len);

	cl_git_fail_with(GIT_ITEROVER, git_tls_ciphers_foreach(&name, &len, &ciphers));
	cl_assert_equal_i(len, 0);
	cl_assert_equal_p(name, NULL);
}

void test_core_tls__two_ciphers(void)
{
	const char *name = NULL;
	size_t len = 0;

	const char *ciphers = "BEST_CIPHER:MY_CIPHER";

	cl_git_pass(git_tls_ciphers_foreach(&name, &len, &ciphers));
	cl_assert_equal_i(len, strlen("BEST_CIPHER"));
	cl_assert_equal_strn(name, "BEST_CIPHER", len);

	cl_git_pass(git_tls_ciphers_foreach(&name, &len, &ciphers));
	cl_assert_equal_i(len, strlen("MY_CIPHER"));
	cl_assert_equal_strn(name, "MY_CIPHER", len);

	cl_git_fail_with(GIT_ITEROVER, git_tls_ciphers_foreach(&name, &len, &ciphers));
	cl_assert_equal_i(len, 0);
	cl_assert_equal_p(name, NULL);
}

void test_core_tls__cipher_lookup(void)
{
	git_tls_cipher cipher;
	char *name = "TLS_RSA_WITH_RC4_128_MD5";

	cl_git_pass(git_tls_cipher_lookup(&cipher, name, strlen(name)));
	cl_assert_equal_s(cipher.nist_name, "TLS_RSA_WITH_RC4_128_MD5");

	cl_git_fail_with(GIT_ENOTFOUND, git_tls_cipher_lookup(&cipher, "DUMMY", 4));
}
