/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_win32_version_h__
#define INCLUDE_win32_version_h__

#include <windows.h>

GIT_INLINE(int) git_has_win32_version(int major, int minor)
{
	WORD wVersion = LOWORD(GetVersion());

	return LOBYTE(wVersion) > major ||
		(LOBYTE(wVersion) == major && HIBYTE(wVersion) >= minor);
}

#endif
