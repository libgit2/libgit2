#include "clar_libgit2.h"
#include "fileops.h"
#include "sysdir.h"
#include "git2/transport.h"
#include <ctype.h>

struct transport_data
{
	unsigned int 
		queried:1,
		inited:1;
};

static int transport_query(unsigned int *out, const char *url, void *param)
{
	struct transport_data *data = param;

	*out = 0;

	data->queried = 1;

	giterr_set_str(-42, "I have decided that I have an error. :(");
	return -69;
}

static int transport_init(
	git_transport **out,
	git_remote *owner,
	void *param)
{
	struct transport_data *data = param;

	data->inited = 1;
	return -1;
}

void test_remote_transport__register(void)
{
	struct transport_data data = {0};
	git_repository *repo;

	cl_git_pass(git_transport_register("foo", transport_query, transport_init, &data));

	cl_git_fail_with(git_clone(&repo, "foo://bar/", "register", NULL), -69);

	cl_assert_equal_i(1, data.queried);

	cl_git_pass(git_transport_unregister("foo"));
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

void test_remote_transport__is_queried(void)
{
	struct transport_data data = {0};
	git_repository *repo;

	cl_git_pass(git_transport_register("foo", transport_query, transport_init, &data));

	cl_git_fail(git_clone(&repo, "foo://bar/", "register", NULL));

	cl_assert_equal_i(1, data.queried);

	cl_git_pass(git_transport_unregister("foo"));
}
