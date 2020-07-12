/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "alloc.h"
#include "runtime.h"

#include "allocators/stdalloc.h"
#include "allocators/win32_crtdbg.h"

#if defined(GIT_MSVC_CRTDBG)
# include "win32/w32_leakcheck.h"
#endif

git_allocator git__allocator;

static int setup_default_allocator(void)
{
#if defined(GIT_MSVC_CRTDBG)
	return git_win32_crtdbg_init_allocator(&git__allocator);
#else
	return git_stdalloc_init_allocator(&git__allocator);
#endif
}

#if defined(GIT_MSVC_CRTDBG)
static void allocator_global_shutdown(void)
{
	git_win32_leakcheck_stacktrace_cleanup();
	git_win32_leakcheck_stack_cleanup();
}
#endif

int git_allocator_global_init(void)
{
#if defined(GIT_MSVC_CRTDBG)
	git_win32_leakcheck_stacktrace_init();
	git_win32_leakcheck_stack_init();

	if (git_runtime_shutdown_register(allocator_global_shutdown) < 0)
		return -1;
#endif

	/*
	 * We don't want to overwrite any allocator which has been set before
	 * the init function is called.
	 */
	if (git__allocator.gmalloc != NULL)
		return 0;

	return setup_default_allocator();
}

int git_allocator_setup(git_allocator *allocator)
{
	if (!allocator)
		return setup_default_allocator();

	memcpy(&git__allocator, allocator, sizeof(*allocator));
	return 0;
}
