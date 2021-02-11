#include "clar_libgit2.h"
#include "remote.h"

/* Suite data */
struct no_proxy_test_entry {
	char url[128];
	char no_proxy[128];
	bool bypass;
};

static struct no_proxy_test_entry no_proxy_test_entries[] = {
	{"https://example.com/", "", false},
	{"https://example.com/", "example.org", false},
	{"https://example.com/", "*", true},
	{"https://example.com/", "example.com,example.org", true},
	{"https://example.com/", ".example.com,example.org", false},
	{"https://foo.example.com/", ".example.com,example.org", true},
	{"https://example.com/", "foo.example.com,example.org", false},

};

void test_remote_no_proxy__entries(void)
{
	unsigned int i;
	git_net_url url = GIT_NET_URL_INIT;
	git_buf no_proxy = GIT_BUF_INIT;
	bool bypass = false;

	for (i = 0; i < ARRAY_SIZE(no_proxy_test_entries); ++i) {
		cl_git_pass(git_net_url_parse(&url, no_proxy_test_entries[i].url));
		cl_git_pass(git_buf_sets(&no_proxy, no_proxy_test_entries[i].no_proxy));
		cl_git_pass(git_remote__get_http_proxy_bypass(&url, &no_proxy, &bypass));

		cl_assert_(bypass == no_proxy_test_entries[i].bypass, no_proxy_test_entries[i].no_proxy);

		git_net_url_dispose(&url);
		git_buf_dispose(&no_proxy);
	}

}
