/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include <git2/common.h>
#include "map.h"

int validate_map_args(
	git_map *out, size_t len, int prot, int flags, int fd, git_off_t offset)
{
	GIT_UNUSED(fd);
	GIT_UNUSED(offset);

	if (out == NULL || len == 0) {
		errno = EINVAL;
		giterr_set(GITERR_OS, "Failed to mmap. No map or zero length");
		return -1;
	}

	if (!(prot & GIT_PROT_WRITE) && !(prot & GIT_PROT_READ)) {
		errno = EINVAL;
		giterr_set(GITERR_OS, "Failed to mmap. Invalid protection parameters");
		return -1;
	}

	if (flags & GIT_MAP_FIXED) {
		errno = EINVAL;
		giterr_set(GITERR_OS, "Failed to mmap. FIXED not set");
		return -1;
	}

	return 0;
}

