/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "../posix.h"

#ifndef GIT_WIN32
ssize_t p_pread(git_file fd, void *data, size_t size, git_off_t offset)
{
	char *b = (char *)data;
	if (!git__is_ssizet(size)) {
		errno = EINVAL;
		return -1;
	}

	while (size > 0) {
		ssize_t r = pread(fd, b, size, offset);
		if (r < 0) {
			if (errno == EINTR || GIT_ISBLOCKED(errno))
				continue;
			return -1;
		}
		if (!r)
			break;

		size -= r;
		offset += r;
		b += r;
	}

	return (b - (char *)data);
}

ssize_t p_pwrite(git_file fd, const void *data, size_t size, git_off_t offset)
{
	char *b = (char *)data;
	if (!git__is_ssizet(size)) {
		errno = EINVAL;
		return -1;
	}

	while (size > 0) {
		ssize_t r = pwrite(fd, b, size, offset);
		if (r < 0) {
			if (errno == EINTR || GIT_ISBLOCKED(errno))
				continue;
			return -1;
		}
		if (!r)
			break;

		size -= r;
		offset += r;
		b += r;
	}

	return (b - (char *)data);
}
#endif
