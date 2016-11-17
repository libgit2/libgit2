/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_cancellation_h__
#define INCLUDE_cancellation_h__

#include "git2/cancellation.h"

#include "global.h"

/**
 * Check whether there's an active cancellation that's been canceled.
 */
GIT_INLINE(bool) git_cancellation__canceled(void)
{
	git_cancellation *c = GIT_GLOBAL->cancellation;

	if (!c)
		return false;

	return git_cancellation_requested(c);
}

#endif
