#ifndef _WIN32
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
#else
# include <ws2tcpip.h>
# ifdef _MSC_VER
#  pragma comment(lib, "ws2_32")
# endif
#endif

#include <locale.h>

#include "clar_libgit2.h"
#include "posix.h"
#include "userdiff.h"

#if LC_ALL > 0
static const char *old_locales[LC_ALL];
#endif

void test_core_posix__initialize(void)
{
#if LC_ALL > 0
	memset(&old_locales, 0, sizeof(old_locales));
#endif

#ifdef GIT_WIN32
	/* on win32, the WSA context needs to be initialized
	 * before any socket calls can be performed */
	WSADATA wsd;

	cl_git_pass(WSAStartup(MAKEWORD(2,2), &wsd));
	cl_assert(LOBYTE(wsd.wVersion) == 2 && HIBYTE(wsd.wVersion) == 2);
#endif
}

void test_core_posix__cleanup(void)
{
    p_unlink("fallocate_test");
}

static bool supports_ipv6(void)
{
#ifdef GIT_WIN32
	/* IPv6 is supported on Vista and newer */
	return git_has_win32_version(6, 0, 0);
#else
	return 1;
#endif
}

void test_core_posix__inet_pton(void)
{
	struct in_addr addr;
	struct in6_addr addr6;
	size_t i;

	struct in_addr_data {
		const char *p;
		const uint8_t n[4];
	};

	struct in6_addr_data {
		const char *p;
		const uint8_t n[16];
	};

	static struct in_addr_data in_addr_data[] = {
		{ "0.0.0.0", { 0, 0, 0, 0 } },
		{ "10.42.101.8", { 10, 42, 101, 8 } },
		{ "127.0.0.1", { 127, 0, 0, 1 } },
		{ "140.177.10.12", { 140, 177, 10, 12 } },
		{ "204.232.175.90", { 204, 232, 175, 90 } },
		{ "255.255.255.255", { 255, 255, 255, 255 } },
	};

	static struct in6_addr_data in6_addr_data[] = {
		{ "::", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
		{ "::1", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } },
		{ "0:0:0:0:0:0:0:1", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } },
		{ "2001:db8:8714:3a90::12", { 0x20, 0x01, 0x0d, 0xb8, 0x87, 0x14, 0x3a, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12 } },
		{ "fe80::f8ba:c2d6:86be:3645", { 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xba, 0xc2, 0xd6, 0x86, 0xbe, 0x36, 0x45 } },
		{ "::ffff:204.152.189.116", { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xcc, 0x98, 0xbd, 0x74 } },
	};

	/* Test some ipv4 addresses */
	for (i = 0; i < 6; i++) {
		cl_assert(p_inet_pton(AF_INET, in_addr_data[i].p, &addr) == 1);
		cl_assert(memcmp(&addr, in_addr_data[i].n, sizeof(struct in_addr)) == 0);
	}

	/* Test some ipv6 addresses */
	if (supports_ipv6())
	{
		for (i = 0; i < 6; i++) {
			cl_assert(p_inet_pton(AF_INET6, in6_addr_data[i].p, &addr6) == 1);
			cl_assert(memcmp(&addr6, in6_addr_data[i].n, sizeof(struct in6_addr)) == 0);
		}
	}

	/* Test some invalid strings */
	cl_assert(p_inet_pton(AF_INET, "", &addr) == 0);
	cl_assert(p_inet_pton(AF_INET, "foo", &addr) == 0);
	cl_assert(p_inet_pton(AF_INET, " 127.0.0.1", &addr) == 0);
	cl_assert(p_inet_pton(AF_INET, "bar", &addr) == 0);
	cl_assert(p_inet_pton(AF_INET, "10.foo.bar.1", &addr) == 0);

	/* Test unsupported address families */
	cl_git_fail(p_inet_pton(INT_MAX-1, "52.472", &addr));
	cl_assert_equal_i(EAFNOSUPPORT, errno);
}

void test_core_posix__utimes(void)
{
	struct p_timeval times[2];
	struct stat st;
	time_t curtime;
	int fd;

	/* test p_utimes */
	times[0].tv_sec = 1234567890;
	times[0].tv_usec = 0;
	times[1].tv_sec = 1234567890;
	times[1].tv_usec = 0;

	cl_git_mkfile("foo", "Dummy file.");
	cl_must_pass(p_utimes("foo", times));

	p_stat("foo", &st);
	cl_assert_equal_i(1234567890, st.st_atime);
	cl_assert_equal_i(1234567890, st.st_mtime);


	/* test p_futimes */
	times[0].tv_sec = 1414141414;
	times[0].tv_usec = 0;
	times[1].tv_sec = 1414141414;
	times[1].tv_usec = 0;

	cl_must_pass(fd = p_open("foo", O_RDWR));
	cl_must_pass(p_futimes(fd, times));
	p_close(fd);

	p_stat("foo", &st);
	cl_assert_equal_i(1414141414, st.st_atime);
	cl_assert_equal_i(1414141414, st.st_mtime);


	/* test p_utimes with current time, assume that
	 * it takes < 5 seconds to get the time...!
	 */
	cl_must_pass(p_utimes("foo", NULL));

	curtime = time(NULL);
	p_stat("foo", &st);
	cl_assert((st.st_atime - curtime) < 5);
	cl_assert((st.st_mtime - curtime) < 5);

	p_unlink("foo");
}

static void try_set_locale(int category)
{
#if LC_ALL > 0
	old_locales[category] = setlocale(category, NULL);
#endif

	if (!setlocale(category, "UTF-8") &&
	    !setlocale(category, "c.utf8") &&
	    !setlocale(category, "en_US.UTF-8"))
		cl_skip();

	if (MB_CUR_MAX == 1)
		cl_fail("Expected locale to be switched to multibyte");
}

void test_core_posix__p_regcomp_ignores_global_locale_ctype(void)
{
	p_regex_t preg;

	try_set_locale(LC_CTYPE);

	cl_assert(!p_regcomp(&preg, "[\xc0-\xff][\x80-\xbf]", P_REG_EXTENDED));

	p_regfree(&preg);
}

void test_core_posix__p_regcomp_ignores_global_locale_collate(void)
{
	p_regex_t preg;

#ifdef GIT_WIN32
	cl_skip();
#endif

	try_set_locale(LC_COLLATE);
	cl_assert(!p_regcomp(&preg, "[\xc0-\xff][\x80-\xbf]", P_REG_EXTENDED));

	p_regfree(&preg);
}

void test_core_posix__p_regcomp_matches_digits_with_locale(void)
{
	p_regex_t preg;
	char c, str[2];

#ifdef GIT_WIN32
	cl_skip();
#endif

	try_set_locale(LC_COLLATE);
	try_set_locale(LC_CTYPE);

	cl_assert(!p_regcomp(&preg, "[[:digit:]]", P_REG_EXTENDED));

	str[1] = '\0';
	for (c = '0'; c <= '9'; c++) {
	    str[0] = c;
	    cl_assert(!p_regexec(&preg, str, 0, NULL, 0));
	}

	p_regfree(&preg);
}

void test_core_posix__p_regcomp_matches_alphabet_with_locale(void)
{
	p_regex_t preg;
	char c, str[2];

#ifdef GIT_WIN32
	cl_skip();
#endif

	try_set_locale(LC_COLLATE);
	try_set_locale(LC_CTYPE);

	cl_assert(!p_regcomp(&preg, "[[:alpha:]]", P_REG_EXTENDED));

	str[1] = '\0';
	for (c = 'a'; c <= 'z'; c++) {
	    str[0] = c;
	    cl_assert(!p_regexec(&preg, str, 0, NULL, 0));
	}
	for (c = 'A'; c <= 'Z'; c++) {
	    str[0] = c;
	    cl_assert(!p_regexec(&preg, str, 0, NULL, 0));
	}

	p_regfree(&preg);
}

void test_core_posix__p_regcomp_compile_userdiff_regexps(void)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(builtin_defs); ++idx) {
		git_diff_driver_definition ddef = builtin_defs[idx];
		int error = 0;
		p_regex_t preg;

		error = p_regcomp(&preg, ddef.fns, P_REG_EXTENDED | ddef.flags);
		p_regfree(&preg);
		cl_assert(!error);

		error = p_regcomp(&preg, ddef.words, P_REG_EXTENDED);
		p_regfree(&preg);
		cl_assert(!error);
	}
}

void test_core_posix__fallocate(void)
{
	int fd;
	struct stat st;

	/* fallocate a new file succeeds */
	cl_must_pass(fd = p_open("fallocate_test", O_RDWR|O_CREAT, 0666));
	cl_must_pass(p_fallocate(fd, 0, 42));
	cl_must_pass(p_fstat(fd, &st));
	cl_assert_equal_i(42, st.st_size);
	p_close(fd);

	/* fallocate an existing file succeeds */
	cl_must_pass(fd = p_open("fallocate_test", O_RDWR, 0666));
	cl_must_pass(p_fallocate(fd, 90, 9));
	cl_must_pass(p_fstat(fd, &st));
	cl_assert_equal_i(99, st.st_size);
	p_close(fd);

	/* fallocate doesn't shrink */
	cl_must_pass(fd = p_open("fallocate_test", O_RDWR, 0666));
	cl_must_pass(p_fallocate(fd, 0, 14));
	cl_must_pass(p_fstat(fd, &st));
	cl_assert_equal_i(99, st.st_size);
	p_close(fd);

	/* fallocate doesn't move the cursor */
	cl_must_pass(fd = p_open("fallocate_test", O_RDWR, 0666));
	cl_must_pass(p_fallocate(fd, 0, 100));
	cl_assert_equal_i(0, p_lseek(fd, 0, SEEK_CUR));
	cl_must_pass(p_lseek(fd, 42, SEEK_SET));
	cl_must_pass(p_fallocate(fd, 0, 200));
	cl_assert_equal_i(42, p_lseek(fd, 0, SEEK_CUR));
	p_close(fd);
}
