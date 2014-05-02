#include "clar_libgit2.h"
#include "fileops.h"
#include "sysdir.h"
#include "git2/transport.h"
#include <ctype.h>

struct transport_data
{
	int query_should_error;
	int query_should_reject;
	int queried;
	int initialized;
};

static int transport_query(
	unsigned int *out,
	const char *scheme,
	const char *url,
	void *param)
{
	struct transport_data *data = param;

	GIT_UNUSED(scheme);
	GIT_UNUSED(url);

	*out = data->query_should_reject ? 0 : 1;

	data->queried = 1;

	if (data->query_should_error)
		giterr_set_str(-42, "I have decided that I have an error. :(");

	return data->query_should_error;
}

static int transport_init(
	git_transport **out,
	const char *scheme,
	const char *url,
	git_remote *owner,
	void *param)
{
	struct transport_data *data = param;

	GIT_UNUSED(out);
	GIT_UNUSED(scheme);
	GIT_UNUSED(url);
	GIT_UNUSED(owner);

	data->initialized = 1;
	return -1;
}

void test_remote_transport__register(void)
{
	struct transport_data data = {0};
	git_repository *repo;

	data.query_should_error = -69;

	cl_git_pass(git_transport_register("foo", transport_query, transport_init, &data));
	cl_git_fail_with(git_clone(&repo, "foo://bar/", "register", NULL), -69);
	cl_assert_equal_i(1, data.queried);
	cl_git_pass(git_transport_unregister("foo"));
}

void test_remote_transport__register_wildcard(void)
{
	struct transport_data data1 = {0}, data2 = {0};
	git_repository *repo;

	data2.query_should_reject = 1;
	data1.query_should_error = -42;

	cl_git_pass(git_transport_register("*", transport_query, transport_init, &data1));
	cl_git_pass(git_transport_register("http", transport_query, transport_init, &data2));

	cl_git_fail_with(git_clone(&repo, "http://bar/", "register", NULL), -42);

	cl_assert_equal_i(1, data1.queried);
	cl_assert_equal_i(1, data2.queried);

	cl_git_pass(git_transport_unregister("*"));
	cl_git_pass(git_transport_unregister("http"));
}

void test_remote_transport__must_unregister_to_reregister(void)
{
	struct transport_data data = {0};

	/* can't reregister */
	cl_git_pass(git_transport_register("foo", transport_query, transport_init, &data));
	cl_git_fail_with(git_transport_register("foo", transport_query, transport_init, &data), GIT_EEXISTS);

	/* can't re-unregister */
	cl_git_pass(git_transport_unregister("foo"));
	cl_git_fail_with(git_transport_unregister("foo"), GIT_ENOTFOUND);

	cl_git_pass(git_transport_register("foo", transport_query, transport_init, &data));
	cl_git_pass(git_transport_unregister("foo"));
}
