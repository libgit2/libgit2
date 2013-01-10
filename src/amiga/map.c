/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include <git2/common.h>

#ifndef GIT_WIN32

#include "posix.h"
#include "map.h"
#include <errno.h>

int p_mmap(git_map *out, size_t len, int prot, int flags, int fd, git_off_t offset)
{
	GIT_MMAP_VALIDATE(out, len, prot, flags);

	out->data = NULL;
	out->len = 0;

	if ((prot & GIT_PROT_WRITE) && ((flags & GIT_MAP_TYPE) == GIT_MAP_SHARED)) {
		giterr_set(GITERR_OS, "Trying to map shared-writeable");
		return -1;
	}

	out->data = malloc(len);
	GITERR_CHECK_ALLOC(out->data);

	if ((p_lseek(fd, offset, SEEK_SET) < 0) || ((size_t)p_read(fd, out->data, len) != len)) {
		giterr_set(GITERR_OS, "mmap emulation failed");
		return -1;
	}

	out->len = len;
	return 0;
}

int p_munmap(git_map *map)
{
	assert(map != NULL);
	free(map->data);

	return 0;
}

#endif

