/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "compress.h"

#include <zlib.h>

#define BUFFER_SIZE (1024 * 1024)

int git__compress(git_buf *buf, const void *buff, size_t len)
{
	z_stream zs;
	char *zb;
	size_t have;

	memset(&zs, 0, sizeof(zs));
	if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK)
		return -1;

	zb = git__malloc(BUFFER_SIZE);
	GITERR_CHECK_ALLOC(zb);

	zs.next_in = (void *)buff;
	zs.avail_in = (uInt)len;

	do {
		zs.next_out = (unsigned char *)zb;
		zs.avail_out = BUFFER_SIZE;

		if (deflate(&zs, Z_FINISH) == Z_STREAM_ERROR) {
			git__free(zb);
			return -1;
		}

		have = BUFFER_SIZE - (size_t)zs.avail_out;

		if (git_buf_put(buf, zb, have) < 0) {
			git__free(zb);
			return -1;
		}

	} while (zs.avail_out == 0);

	assert(zs.avail_in == 0);

	deflateEnd(&zs);
	git__free(zb);
	return 0;
}
