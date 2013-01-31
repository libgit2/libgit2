#include "clar_libgit2.h"
#include "netops.h"

void test_network_urlparse__trivial(void)
{
	char *host, *port, *user, *pass;

	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_sz(user, NULL);
	cl_assert_equal_sz(pass, NULL);
}

void test_network_urlparse__user(void)
{
	char *host, *port, *user, *pass;

	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"user@example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_sz(pass, NULL);
}

void test_network_urlparse__user_pass(void)
{
	char *host, *port, *user, *pass;

	/* user:pass@hostname.tld/resource */
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"user:pass@example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_s(pass, "pass");
}

void test_network_urlparse__port(void)
{
	char *host, *port, *user, *pass;

	/* hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_sz(user, NULL);
	cl_assert_equal_sz(pass, NULL);
}

void test_network_urlparse__user_port(void)
{
	char *host, *port, *user, *pass;

	/* user@hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"user@example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_sz(pass, NULL);
}

void test_network_urlparse__user_pass_port(void)
{
	char *host, *port, *user, *pass;

	/* user:pass@hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"user:pass@example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_s(pass, "pass");
}
