#include "clar_libgit2.h"
#include "net.h"

struct url_pattern {
	const char *url;
	const char *pattern;
	bool matches;
};

void test_network_url_pattern__single(void)
{
	git_net_url url;
	size_t i;

	struct url_pattern url_patterns[] = {
		/* Wildcard matches */
		{ "https://example.com/", "", false },
		{ "https://example.com/", "*", true },

		/* Literal and wildcard matches */
		{ "https://example.com/", "example.com", true },
		{ "https://example.com/", ".example.com", true },
		{ "https://example.com/", "*.example.com", true },
		{ "https://www.example.com/", "www.example.com", true },
		{ "https://www.example.com/", ".example.com", true },
		{ "https://www.example.com/", "*.example.com", true },

		/* Literal and wildcard failures */
		{ "https://example.com/", "example.org", false },
		{ "https://example.com/", ".example.org", false },
		{ "https://example.com/", "*.example.org", false },
		{ "https://foo.example.com/", "www.example.com", false },

		/*
		 * A port in the pattern is optional; if no port is
		 * present, it matches *all* ports.
		 */
		{ "https://example.com/", "example.com:443", true },
		{ "https://example.com/", "example.com:80", false },
		{ "https://example.com:1443/", "example.com", true },

		/* Failures with similar prefix/suffix */
		{ "https://texample.com/", "example.com", false },
		{ "https://example.com/", "mexample.com", false },
		{ "https://example.com:44/", "example.com:443", false },
		{ "https://example.com:443/", "example.com:44", false },
	};

	for (i = 0; i < ARRAY_SIZE(url_patterns); i++) {
		cl_git_pass(git_net_url_parse(&url, url_patterns[i].url));
		cl_assert_(git_net_url_matches_pattern(&url, url_patterns[i].pattern) == url_patterns[i].matches, url_patterns[i].pattern);
		git_net_url_dispose(&url);
	}
}
