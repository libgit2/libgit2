/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "../posix.h"

#ifndef GIT_WIN32

#ifdef USE_VECTORED_IO
#include <sys/uio.h>

static int prepare_iovec(uint8_t *data, size_t size, struct iovec *iov,
		int count) {
	int cc;
	size_t chunk;
	size_t page_size;

	git__page_size(&page_size);
	count = min((size > page_size) ? (int)(size/page_size) : 1, count);
	chunk = size / count;

	for (cc = 0; cc < count; ++cc) {
		iov[cc].iov_base = &data[chunk * cc];
		iov[cc].iov_len = chunk;
	}

	/* Last chunk reads the extra bytes beyond a chunk */
	if (count > 1) {
		iov[count - 1].iov_len = chunk + (size % chunk);
	}

	return count;
}
#endif

ssize_t p_pread(git_file fd, void *data, size_t size, git_off_t offset)
{
	char *b = (char *)data;

#ifdef USE_VECTORED_IO
	struct iovec iov[8];
	int iov_count = prepare_iovec((uint8_t *)data, size, iov,
			sizeof(iov)/sizeof(iov[0]));
#endif

	if (!git__is_ssizet(size)) {
		errno = EINVAL;
		return -1;
	}

	while (size > 0) {
#ifdef USE_VECTORED_IO
		ssize_t r = preadv(fd, iov, iov_count, offset);
#else
		ssize_t r = pread(fd, b, size, offset);
#endif
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

#ifdef USE_VECTORED_IO
	struct iovec iov[8];
	int iov_count = prepare_iovec((uint8_t *)data, size, iov,
			sizeof(iov)/sizeof(iov[0]));
#endif

	if (!git__is_ssizet(size)) {
		errno = EINVAL;
		return -1;
	}

	while (size > 0) {
#ifdef USE_VECTORED_IO
		ssize_t r = pwritev(fd, iov, iov_count, offset);
#else
		ssize_t r = pwrite(fd, b, size, offset);
#endif
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
