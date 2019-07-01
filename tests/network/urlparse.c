#include "clar_libgit2.h"
#include "net.h"

static git_net_url conndata;

void test_network_urlparse__initialize(void)
{
	memset(&conndata, 0, sizeof(conndata));
}

void test_network_urlparse__cleanup(void)
{
	git_net_url_dispose(&conndata);
}

/*
 * example.com based tests
 */

void test_network_urlparse__trivial(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://example.com/resource"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__root(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://example.com/"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__implied_root(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://example.com"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__implied_root_custom_port(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://example.com:42"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "42");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__implied_root_empty_port(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://example.com:"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__encoded_password(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
				"https://user:pass%2fis%40bad@hostname.com:1234/"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "hostname.com");
	cl_assert_equal_s(conndata.port, "1234");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass/is@bad");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__user(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
				"https://user@example.com/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "443");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__user_pass(void)
{
	/* user:pass@hostname.tld/resource */
	cl_git_pass(git_net_url_parse(&conndata,
				"https://user:pass@example.com/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "443");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__port(void)
{
	/* hostname.tld:port/resource */
	cl_git_pass(git_net_url_parse(&conndata,
				"https://example.com:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__empty_port(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://example.com:/resource"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__user_port(void)
{
	/* user@hostname.tld:port/resource */
	cl_git_pass(git_net_url_parse(&conndata,
				"https://user@example.com:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__user_pass_port(void)
{
	/* user:pass@hostname.tld:port/resource */
	cl_git_pass(git_net_url_parse(&conndata,
				"https://user:pass@example.com:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

/*
 * IPv4 based tests
 */

void test_network_urlparse__trivial_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://192.168.1.1/resource"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__root_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://192.168.1.1/"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__implied_root_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://192.168.1.1"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__implied_root_custom_port_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://192.168.1.1:42"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "42");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__implied_root_empty_port_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://192.168.1.1:"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__encoded_password_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user:pass%2fis%40bad@192.168.1.1:1234/"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "1234");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass/is@bad");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__user_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user@192.168.1.1/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "443");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__user_pass_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user:pass@192.168.1.1/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "443");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__port_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://192.168.1.1:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__empty_port_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://192.168.1.1:/resource"));
	cl_assert_equal_s(conndata.scheme, "http");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);
}

void test_network_urlparse__user_port_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user@192.168.1.1:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

void test_network_urlparse__user_pass_port_ipv4(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user:pass@192.168.1.1:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
	cl_assert_equal_s(conndata.host, "192.168.1.1");
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);
}

/*
 * IPv6 based tests
 */

void test_network_urlparse__trivial_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://[fe80::dcad:beff:fe00:0001]/resource"));
	cl_assert_equal_s(conndata.scheme, "http");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001]/resource"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://[fe80::dcad:beff:fe00:0001/resource"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001/resource"));
#endif
}

void test_network_urlparse__root_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://[fe80::dcad:beff:fe00:0001]/"));
	cl_assert_equal_s(conndata.scheme, "http");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001]/"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://[fe80::dcad:beff:fe00:0001/"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001/"));
#endif
}

void test_network_urlparse__implied_root_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://[fe80::dcad:beff:fe00:0001]"));
	cl_assert_equal_s(conndata.scheme, "http");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001]"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://[fe80::dcad:beff:fe00:0001"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001"));
#endif
}

void test_network_urlparse__implied_root_custom_port_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://[fe80::dcad:beff:fe00:0001]:42"));
	cl_assert_equal_s(conndata.scheme, "http");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "42");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001]:42"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://[fe80::dcad:beff:fe00:0001:42"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001:42"));
#endif
}

void test_network_urlparse__implied_root_empty_port_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://[fe80::dcad:beff:fe00:0001]:"));
	cl_assert_equal_s(conndata.scheme, "http");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001]:"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://[fe80::dcad:beff:fe00:0001:"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001:"));
#endif
}

void test_network_urlparse__encoded_password_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user:pass%2fis%40bad@[fe80::dcad:beff:fe00:0001]:1234/"));
	cl_assert_equal_s(conndata.scheme, "https");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "1234");
	cl_assert_equal_s(conndata.path, "/");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass/is@bad");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass%2fis%40bad@fe80::dcad:beff:fe00:0001]:1234/"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass%2fis%40bad@[fe80::dcad:beff:fe00:0001:1234/"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass%2fis%40bad@fe80::dcad:beff:fe00:0001:1234/"));
#endif
}

void test_network_urlparse__user_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user@[fe80::dcad:beff:fe00:0001]/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "443");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user@fe80::dcad:beff:fe00:0001]/resource"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user@[fe80::dcad:beff:fe00:0001/resource"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user@fe80::dcad:beff:fe00:0001/resource"));
#endif
}

void test_network_urlparse__user_pass_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user:pass@[fe80::dcad:beff:fe00:0001]/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "443");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass@fe80::dcad:beff:fe00:0001]/resource"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass@[fe80::dcad:beff:fe00:0001/resource"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass@fe80::dcad:beff:fe00:0001/resource"));
#endif
}

void test_network_urlparse__port_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://[fe80::dcad:beff:fe00:0001]:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://fe80::dcad:beff:fe00:0001]:9191/resource"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://[fe80::dcad:beff:fe00:0001:9191/resource"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://fe80::dcad:beff:fe00:0001:9191/resource"));
#endif
}

void test_network_urlparse__empty_port_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata, "http://[fe80::dcad:beff:fe00:0001]:/resource"));
	cl_assert_equal_s(conndata.scheme, "http");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_p(conndata.username, NULL);
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 1);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001]:/resource"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://[fe80::dcad:beff:fe00:0001:/resource"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"http://fe80::dcad:beff:fe00:0001:/resource"));
#endif
}

void test_network_urlparse__user_port_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user@[fe80::dcad:beff:fe00:0001]:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_p(conndata.password, NULL);
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user@fe80::dcad:beff:fe00:0001]:9191/resource"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user@[fe80::dcad:beff:fe00:0001:9191/resource"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user@fe80::dcad:beff:fe00:0001:9191/resource"));
#endif
}

void test_network_urlparse__user_pass_port_ipv6(void)
{
	cl_git_pass(git_net_url_parse(&conndata,
		"https://user:pass@[fe80::dcad:beff:fe00:0001]:9191/resource"));
	cl_assert_equal_s(conndata.scheme, "https");
#ifdef WIN32
	cl_assert_equal_s(conndata.host, "[fe80::dcad:beff:fe00:0001]");
#else
	cl_assert_equal_s(conndata.host, "fe80::dcad:beff:fe00:0001");
#endif
	cl_assert_equal_s(conndata.port, "9191");
	cl_assert_equal_s(conndata.path, "/resource");
	cl_assert_equal_s(conndata.username, "user");
	cl_assert_equal_s(conndata.password, "pass");
	cl_assert_equal_i(git_net_url_is_default_port(&conndata), 0);

	/* Opening bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass@fe80::dcad:beff:fe00:0001]:9191/resource"));
	/* Closing bracket missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass@[fe80::dcad:beff:fe00:0001:9191/resource"));
#ifdef WIN32
	/* Both brackets missing */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata,
		"https://user:pass@fe80::dcad:beff:fe00:0001:9191/resource"));
#endif
}

void test_network_urlparse__fails_ipv6(void)
{
	/* Invalid chracter inside address */
	cl_git_fail_with(GIT_EINVALIDSPEC, git_net_url_parse(&conndata, "http://[fe8o::dcad:beff:fe00:0001]/resource"));
}
