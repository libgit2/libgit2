#include "clar_libgit2.h"

void test_core_features__basic(void)
{
	int caps = git_libgit2_features();

#ifdef GIT_THREADS
	cl_assert((caps & GIT_FEATURE_THREADS) != 0);
#else
	cl_assert((caps & GIT_FEATURE_THREADS) == 0);
#endif

#ifdef GIT_HTTPS
	cl_assert((caps & GIT_FEATURE_HTTPS) != 0);
#endif

#if defined(GIT_SSH)
	cl_assert((caps & GIT_FEATURE_SSH) != 0);
#else
	cl_assert((caps & GIT_FEATURE_SSH) == 0);
#endif

#if defined(GIT_USE_NSEC)
	cl_assert((caps & GIT_FEATURE_NSEC) != 0);
#else
	cl_assert((caps & GIT_FEATURE_NSEC) == 0);
#endif

	cl_assert((caps & GIT_FEATURE_HTTP_PARSER) != 0);
	cl_assert((caps & GIT_FEATURE_REGEX) != 0);

#if defined(GIT_USE_ICONV)
	cl_assert((caps & GIT_FEATURE_I18N) != 0);
#endif

#if defined(GIT_NTLM) || defined(GIT_WIN32)
	cl_assert((caps & GIT_FEATURE_AUTH_NTLM) != 0);
#endif
#if defined(GIT_GSSAPI) || defined(GIT_GSSFRAMEWORK) || defined(GIT_WIN32)
	cl_assert((caps & GIT_FEATURE_AUTH_NEGOTIATE) != 0);
#endif

	cl_assert((caps & GIT_FEATURE_COMPRESSION) != 0);
	cl_assert((caps & GIT_FEATURE_SHA1) != 0);

#if defined(GIT_EXPERIMENTAL_SHA256)
	cl_assert((caps & GIT_FEATURE_SHA256) != 0);
#endif

	/*
	 * Ensure that our tests understand all the features;
	 * this test tries to ensure that if there's a new feature
	 * added that the backends test (below) is updated as well.
	 */
	cl_assert((caps & ~(GIT_FEATURE_THREADS |
	                    GIT_FEATURE_HTTPS |
	                    GIT_FEATURE_SSH |
	                    GIT_FEATURE_NSEC |
	                    GIT_FEATURE_HTTP_PARSER |
	                    GIT_FEATURE_REGEX |
	                    GIT_FEATURE_I18N |
	                    GIT_FEATURE_AUTH_NTLM |
	                    GIT_FEATURE_AUTH_NEGOTIATE |
	                    GIT_FEATURE_COMPRESSION |
	                    GIT_FEATURE_SHA1 |
	                    GIT_FEATURE_SHA256
			    )) == 0);
}

void test_core_features__backends(void)
{
	const char *threads = git_libgit2_feature_backend(GIT_FEATURE_THREADS);
	const char *https = git_libgit2_feature_backend(GIT_FEATURE_HTTPS);
	const char *ssh = git_libgit2_feature_backend(GIT_FEATURE_SSH);
	const char *nsec = git_libgit2_feature_backend(GIT_FEATURE_NSEC);
	const char *http_parser = git_libgit2_feature_backend(GIT_FEATURE_HTTP_PARSER);
	const char *regex = git_libgit2_feature_backend(GIT_FEATURE_REGEX);
	const char *i18n = git_libgit2_feature_backend(GIT_FEATURE_I18N);
	const char *ntlm = git_libgit2_feature_backend(GIT_FEATURE_AUTH_NTLM);
	const char *negotiate = git_libgit2_feature_backend(GIT_FEATURE_AUTH_NEGOTIATE);
	const char *compression = git_libgit2_feature_backend(GIT_FEATURE_COMPRESSION);
	const char *sha1 = git_libgit2_feature_backend(GIT_FEATURE_SHA1);
	const char *sha256 = git_libgit2_feature_backend(GIT_FEATURE_SHA256);

	git_buf threads_opt = GIT_BUF_INIT;
	git_buf https_opt = GIT_BUF_INIT;
	git_buf ssh_opt = GIT_BUF_INIT;
	git_buf nsec_opt = GIT_BUF_INIT;
	git_buf http_parser_opt = GIT_BUF_INIT;
	git_buf regex_opt = GIT_BUF_INIT;
	git_buf i18n_opt = GIT_BUF_INIT;
	git_buf ntlm_opt = GIT_BUF_INIT;
	git_buf negotiate_opt = GIT_BUF_INIT;
	git_buf compression_opt = GIT_BUF_INIT;
	git_buf sha1_opt = GIT_BUF_INIT;
	git_buf sha256_opt = GIT_BUF_INIT;

	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_THREADS,        &threads_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_HTTPS,          &https_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_SSH,            &ssh_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_NSEC,           &nsec_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_HTTP_PARSER,    &http_parser_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_REGEX,          &regex_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_I18N,           &i18n_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_AUTH_NTLM,      &ntlm_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_AUTH_NEGOTIATE, &negotiate_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_COMPRESSION,    &compression_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_SHA1,           &sha1_opt));
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_BACKEND, GIT_FEATURE_SHA256,         &sha256_opt));

#if defined(GIT_THREADS) && defined(GIT_WIN32)
	cl_assert_equal_s("win32", threads);
#elif defined(GIT_THREADS)
	cl_assert_equal_s("pthread", threads);
#else
	cl_assert(threads == NULL);
#endif
	cl_assert_equal_s(threads? threads: "", threads_opt.ptr); /* non-changeable backend */

#if defined(GIT_HTTPS) && defined(GIT_OPENSSL)
	cl_assert_equal_s("openssl", https);
#elif defined(GIT_HTTPS) && defined(GIT_OPENSSL_DYNAMIC)
	cl_assert_equal_s("openssl-dynamic", https);
#elif defined(GIT_HTTPS) && defined(GIT_MBEDTLS)
	cl_assert_equal_s("mbedtls", https);
#elif defined(GIT_HTTPS) && defined(GIT_SECURE_TRANSPORT)
	cl_assert_equal_s("securetransport", https);
#elif defined(GIT_HTTPS) && defined(GIT_SCHANNEL)
	cl_assert_equal_s("schannel", https);
#elif defined(GIT_HTTPS) && defined(GIT_WINHTTP)
	cl_assert_equal_s("winhttp", https);
#elif defined(GIT_HTTPS)
	cl_assert(0);
#else
	cl_assert(https == NULL);
#endif
	cl_assert_equal_s(https? https: "", https_opt.ptr); /* non-changeable backend */

#if defined(GIT_SSH) && defined(GIT_SSH_LIBSSH2) && defined(GIT_SSH_EXEC)
	cl_assert_equal_s("libssh2,exec", ssh);
	cl_assert_equal_s("libssh2", ssh_opt.ptr);
#elif defined(GIT_SSH) && defined(GIT_SSH_EXEC)
	cl_assert_equal_s("exec", ssh);
	cl_assert_equal_s("exec", opt_ssh.ptr);
#elif defined(GIT_SSH) && defined(GIT_SSH_LIBSSH2)
	cl_assert_equal_s("libssh2", ssh);
	cl_assert_equal_s("libssh2", opt_ssh.ptr);
#elif defined(GIT_SSH)
	cl_assert(0);
#else
	cl_assert(ssh == NULL);
	cl_assert_equal_s("", opt_ssh.ptr);
#endif

#if defined(GIT_USE_NSEC) && defined(GIT_USE_STAT_MTIMESPEC)
	cl_assert_equal_s("mtimespec", nsec);
#elif defined(GIT_USE_NSEC) && defined(GIT_USE_STAT_MTIM)
	cl_assert_equal_s("mtim", nsec);
#elif defined(GIT_USE_NSEC) && defined(GIT_USE_STAT_MTIME_NSEC)
	cl_assert_equal_s("mtime", nsec);
#elif defined(GIT_USE_NSEC) && defined(GIT_WIN32)
	cl_assert_equal_s("win32", nsec);
#elif defined(GIT_USE_NSEC)
	cl_assert(0);
#else
	cl_assert(nsec == NULL);
#endif
	cl_assert_equal_s(nsec? nsec: "", nsec_opt.ptr); /* non-changeable backend */

#if defined(GIT_HTTPPARSER_HTTPPARSER)
	cl_assert_equal_s("httpparser", http_parser);
#elif defined(GIT_HTTPPARSER_LLHTTP)
	cl_assert_equal_s("llhttp", http_parser);
#elif defined(GIT_HTTPPARSER_BUILTIN)
	cl_assert_equal_s("builtin", http_parser);
#else
	cl_assert(0);
#endif
	cl_assert_equal_s(http_parser? http_parser: "", http_parser_opt.ptr); /* non-changeable backend */

#if defined(GIT_REGEX_REGCOMP_L)
	cl_assert_equal_s("regcomp_l", regex);
#elif defined(GIT_REGEX_REGCOMP)
	cl_assert_equal_s("regcomp", regex);
#elif defined(GIT_REGEX_PCRE)
	cl_assert_equal_s("pcre", regex);
#elif defined(GIT_REGEX_PCRE2)
	cl_assert_equal_s("pcre2", regex);
#elif defined(GIT_REGEX_BUILTIN)
	cl_assert_equal_s("builtin", regex);
#else
	cl_assert(0);
#endif
	cl_assert_equal_s(regex? regex: "", regex_opt.ptr); /* non-changeable backend */

#if defined(GIT_USE_ICONV)
	cl_assert_equal_s("iconv", i18n);
#else
	cl_assert(i18n == NULL);
#endif
	cl_assert_equal_s(i18n? i18n: "", i18n_opt.ptr); /* non-changeable backend */

#if defined(GIT_NTLM)
	cl_assert_equal_s("ntlmclient", ntlm);
#elif defined(GIT_WIN32)
	cl_assert_equal_s("sspi", ntlm);
#else
	cl_assert(ntlm == NULL);
#endif
	cl_assert_equal_s(ntlm? ntlm: "", ntlm_opt.ptr); /* non-changeable backend */

#if defined(GIT_GSSAPI)
	cl_assert_equal_s("gssapi", negotiate);
#elif defined(GIT_WIN32)
	cl_assert_equal_s("sspi", negotiate);
#else
	cl_assert(negotiate == NULL);
#endif
	cl_assert_equal_s(negotiate? negotiate: "", negotiate_opt.ptr); /* non-changeable backend */

#if defined(GIT_COMPRESSION_BUILTIN)
	cl_assert_equal_s("builtin", compression);
#elif defined(GIT_COMPRESSION_ZLIB)
	cl_assert_equal_s("zlib", compression);
#else
	cl_assert(0);
#endif
	cl_assert_equal_s(compression? compression: "", compression_opt.ptr); /* non-changeable backend */

#if defined(GIT_SHA1_COLLISIONDETECT)
	cl_assert_equal_s("builtin", sha1);
#elif defined(GIT_SHA1_OPENSSL)
	cl_assert_equal_s("openssl", sha1);
#elif defined(GIT_SHA1_OPENSSL_FIPS)
	cl_assert_equal_s("openssl-fips", sha1);
#elif defined(GIT_SHA1_OPENSSL_DYNAMIC)
	cl_assert_equal_s("openssl-dynamic", sha1);
#elif defined(GIT_SHA1_MBEDTLS)
	cl_assert_equal_s("mbedtls", sha1);
#elif defined(GIT_SHA1_COMMON_CRYPTO)
	cl_assert_equal_s("commoncrypto", sha1);
#elif defined(GIT_SHA1_WIN32)
	cl_assert_equal_s("win32", sha1);
#else
	cl_assert(0);
#endif
	cl_assert_equal_s(sha1? sha1: "", sha1_opt.ptr); /* non-changeable backend */

#if defined(GIT_EXPERIMENTAL_SHA256) && defined(GIT_SHA256_BUILTIN)
	cl_assert_equal_s("builtin", sha256);
#elif defined(GIT_EXPERIMENTAL_SHA256) && defined(GIT_SHA256_OPENSSL)
	cl_assert_equal_s("openssl", sha256);
#elif defined(GIT_EXPERIMENTAL_SHA256) && defined(GIT_SHA256_OPENSSL_FIPS)
	cl_assert_equal_s("openssl-fips", sha256);
#elif defined(GIT_EXPERIMENTAL_SHA256) && defined(GIT_SHA256_OPENSSL_DYNAMIC)
	cl_assert_equal_s("openssl-dynamic", sha256);
#elif defined(GIT_EXPERIMENTAL_SHA256) && defined(GIT_SHA256_MBEDTLS)
	cl_assert_equal_s("mbedtls", sha256);
#elif defined(GIT_EXPERIMENTAL_SHA256) && defined(GIT_SHA256_COMMON_CRYPTO)
	cl_assert_equal_s("commoncrypto", sha256);
#elif defined(GIT_EXPERIMENTAL_SHA256) && defined(GIT_SHA256_WIN32)
	cl_assert_equal_s("win32", sha256);
#elif defined(GIT_EXPERIMENTAL_SHA256)
	cl_assert(0);
#else
	cl_assert(sha256 == NULL);
#endif
	cl_assert_equal_s(sha256? sha256: "", sha256_opt.ptr); /* non-changeable backend */

	git_buf_dispose(&threads_opt);
	git_buf_dispose(&https_opt);
	git_buf_dispose(&ssh_opt);
	git_buf_dispose(&nsec_opt);
	git_buf_dispose(&http_parser_opt);
	git_buf_dispose(&regex_opt);
	git_buf_dispose(&i18n_opt);
	git_buf_dispose(&ntlm_opt);
	git_buf_dispose(&negotiate_opt);
	git_buf_dispose(&compression_opt);
	git_buf_dispose(&sha1_opt);
	git_buf_dispose(&sha256_opt);
}
