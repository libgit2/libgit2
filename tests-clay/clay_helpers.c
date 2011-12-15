#include "clay_libgit2.h"

void clay_on_init(void)
{
	git_threads_init();
}

void clay_on_shutdown(void)
{
	git_threads_shutdown();
}
