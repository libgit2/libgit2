#include "clar_libgit2.h"
#include "netops.h"

char *host, *port, *user, *pass;
gitno_connection_data conndata;

void test_network_urlparse__initialize(void)
{
	host = port = user = pass = NULL;
	memset(&conndata, 0, sizeof(conndata));
}

void test_network_urlparse__cleanup(void)
{
#define FREE_AND_NULL(x) if (x) { git__free(x); x = NULL; }
	FREE_AND_NULL(host);
	FREE_AND_NULL(port);
	FREE_AND_NULL(user);
	FREE_AND_NULL(pass);

	gitno_connection_data_free_ptrs(&conndata);
}

void test_network_urlparse__trivial(void)
{
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_p(user, NULL);
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user(void)
{
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"user@example.com/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "8080");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user_pass(void)
{
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
	/* hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_p(user, NULL);
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user_port(void)
{
	/* user@hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"user@example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_p(pass, NULL);
}

void test_network_urlparse__user_pass_port(void)
{
	/* user:pass@hostname.tld:port/resource */
	cl_git_pass(gitno_extract_url_parts(&host, &port, &user, &pass,
				"user:pass@example.com:9191/resource", "8080"));
	cl_assert_equal_s(host, "example.com");
	cl_assert_equal_s(port, "9191");
	cl_assert_equal_s(user, "user");
	cl_assert_equal_s(pass, "pass");
}

void test_network_urlparse__connection_data_http(void)
{
	cl_git_pass(gitno_connection_data_from_url(&conndata,
				"http://example.com/foo/bar/baz", "bar/baz", NULL, false));
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "80");
	cl_assert_equal_s(conndata.path, "/foo/");
	cl_assert_equal_p(conndata.user, NULL);
	cl_assert_equal_p(conndata.pass, NULL);
	cl_assert_equal_i(conndata.use_ssl, false);
}

void test_network_urlparse__connection_data_ssl(void)
{
	cl_git_pass(gitno_connection_data_from_url(&conndata,
				"https://example.com/foo/bar/baz", "bar/baz", NULL, false));
	cl_assert_equal_s(conndata.host, "example.com");
	cl_assert_equal_s(conndata.port, "443");
	cl_assert_equal_s(conndata.path, "/foo/");
	cl_assert_equal_p(conndata.user, NULL);
	cl_assert_equal_p(conndata.pass, NULL);
	cl_assert_equal_i(conndata.use_ssl, true);
}

void test_network_urlparse__connection_data_cross_host_redirect(void)
{
	cl_git_fail_with(gitno_connection_data_from_url(&conndata,
				"https://foo.com/bar/baz", NULL, "bar.com", true),
			-1);
}

void test_network_urlparse__connection_data_http_downgrade(void)
{
	cl_git_fail_with(gitno_connection_data_from_url(&conndata,
				"http://foo.com/bar/baz", NULL, NULL, true),
			-1);
}
