#include "clar_libgit2.h"
#include "netops.h"

char *prot, *host, *port, *user, *pass;

void test_network_urlparse__initialize(void)
{
	prot = host = port = user = pass = NULL;
}

void test_network_urlparse__cleanup(void)
{
#define FREE_AND_NULL(x) if (x) { git__free(x); x = NULL; }
	FREE_AND_NULL(host);
	FREE_AND_NULL(port);
	FREE_AND_NULL(user);
	FREE_AND_NULL(pass);
}

void test_network_urlparse__trivial(void)
{
	cl_git_pass(gitno_extract_url_parts(NULL, &host, &port, &user, &pass,
				"example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_p(user, NULL);
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user(void)
{
	cl_git_pass(gitno_extract_url_parts(NULL, &host, &port, &user, &pass,
				"user@example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user_pass(void)
{
	/* user:pass@hostname.tld/resource */
	cl_git_pass(gitno_extract_url_parts(NULL, &host, &port, &user, &pass,
				"user:pass@example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_s(pass, "pass");
}

void test_network_urlparse__port(void)
{
	/* hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(NULL, &host, &port, &user, &pass,
				"example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_p(user, NULL);
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user_port(void)
{
	/* user@hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(&prot, &host, &port, &user, &pass,
				"ssh://user@example.com:9191/resource", "8080"));
	cl_assert_equal_s(prot, "ssh");
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user_pass_port(void)
{
	/* user:pass@hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(NULL, &host, &port, &user, &pass,
				"user:pass@example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_s(pass, "pass");
}

void test_network_urlparse__protocol(void)
{
	cl_git_pass(gitno_extract_url_parts(&prot, &host, &port, &user, &pass,
				"someprotocol://someplace/something", ""));
	cl_assert_equal_s(prot, "someprotocol");
	cl_assert_equal_s(host, "someplace");
}
