#include "clar_libgit2.h"
#include "clar_libgit2_trace.h"

#ifdef _WIN32
int __cdecl main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	int res;

	clar_test_init(argc, argv);

	cl_global_trace_register();

	clar_register_global_fn(cl_git_global_init, cl_git_global_cleanup);

	/* Run the test suite */
	res = clar_test_run();

	clar_test_shutdown();

	cl_global_trace_disable();
	git_libgit2_shutdown();

	return res;
}
