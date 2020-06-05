/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <git2.h>
#include <git2client.h>
#include "git2_util.h"
#include "runtime.h"
#include "alloc.h"
#include "hash.h"

#ifdef GIT_WIN32
# include "win32/w32_crtdbg_stacktrace.h"
# include "win32/w32_stack.h"
#endif

static void libgit2_shutdown(void);

static int libgit2_init(void)
{
	if (git_libgit2_init() < 0)
		return -1;

	return git_runtime_shutdown_register(libgit2_shutdown);
}

static void libgit2_shutdown(void)
{
	git_libgit2_shutdown();
}

int git_client_init(void)
{
	static git_runtime_init_fn init_fns[] = {
		libgit2_init,

#if defined(GIT_MSVC_CRTDBG)
		git_win32__crtdbg_stacktrace_init,
		git_win32__stack_init,
#endif
		git_allocator_global_init,
		git_threads_global_init,
		git_hash_global_init,
	};

	return git_runtime_init(init_fns, ARRAY_SIZE(init_fns));
}

int git_client_shutdown(void)
{
	return git_runtime_shutdown();
}
