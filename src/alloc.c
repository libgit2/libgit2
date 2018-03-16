/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "alloc.h"

#if defined(GIT_MSVC_CRTDBG)
# include "win32/w32_crtdbg_stacktrace.h"
#else
# include "stdalloc.h"
#endif

git_allocator git__allocator;

int git_allocator_global_init(void)
{
#if defined(GIT_MSVC_CRTDBG)
	return git_win32_crtdbg_init_allocator(&git__allocator);
#else
	return git_stdalloc_init_allocator(&git__allocator);
#endif
}

int git_allocator_setup(git_allocator *allocator)
{
	memcpy(&git__allocator, allocator, sizeof(*allocator));
	return 0;
}

#if !defined(GIT_MSVC_CRTDBG)
int git_win32_crtdbg_init_allocator(git_allocator *allocator)
{
	GIT_UNUSED(allocator);
	giterr_set(GIT_EINVALID, "crtdbg memory allocator not available");
	return -1;
}
#endif
