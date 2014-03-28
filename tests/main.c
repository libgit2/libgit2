#include "clar_libgit2.h"

#ifdef _WIN32
int __cdecl main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	const char *sandbox_path;
	int res;

	clar_test_init(argc, argv);

	git_threads_init();

	sandbox_path = clar_sandbox_path();
	git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, sandbox_path);
	git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_XDG, sandbox_path);

	/* Run the test suite */
	res = clar_test_run();

	clar_test_shutdown();

	giterr_clear();
	git_threads_shutdown();

	return res;
}
