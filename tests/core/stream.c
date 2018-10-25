#include "clar_libgit2.h"
#include "git2/sys/stream.h"
#include "streams/tls.h"
#include "stream.h"

static git_stream test_stream;
static int ctor_called;

static int test_stream_init(git_stream **out, const char *host, const char *port)
{
	GIT_UNUSED(host);
	GIT_UNUSED(port);

	ctor_called = 1;
	*out = &test_stream;

	return 0;
}

static int test_stream_wrap(git_stream **out, git_stream *in, const char *host)
{
	GIT_UNUSED(in);
	GIT_UNUSED(host);

	ctor_called = 1;
	*out = &test_stream;

	return 0;
}

void test_core_stream__register_tls(void)
{
	git_stream *stream;
	git_stream_registration registration = {0};
	int error;

	registration.version = 1;
	registration.init = test_stream_init;
	registration.wrap = test_stream_wrap;

	ctor_called = 0;
	cl_git_pass(git_stream_register_tls(&registration));
	cl_git_pass(git_tls_stream_new(&stream, "localhost", "443"));
	cl_assert_equal_i(1, ctor_called);
	cl_assert_equal_p(&test_stream, stream);

	ctor_called = 0;
	stream = NULL;
	cl_git_pass(git_stream_register_tls(NULL));
	error = git_tls_stream_new(&stream, "localhost", "443");

	/* We don't have TLS support enabled, or we're on Windows,
	 * which has no arbitrary TLS stream support.
	 */
#if defined(GIT_WIN32) || !defined(GIT_HTTPS)
	cl_git_fail_with(-1, error);
#else
	cl_git_pass(error);
#endif

	cl_assert_equal_i(0, ctor_called);
	cl_assert(&test_stream != stream);

	git_stream_free(stream);
}
