/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <zlib.h>

#include "zstream.h"
#include "buffer.h"

#define BUFFER_SIZE (1024 * 1024)

static int zstream_seterr(int zerr, git_zstream *zstream)
{
	if (zerr == Z_MEM_ERROR)
		giterr_set_oom();
	else if (zstream->msg)
		giterr_set(GITERR_ZLIB, zstream->msg);
	else
		giterr_set(GITERR_ZLIB, "Unknown compression error");

	return -1;
}

int git_zstream_init(git_zstream *zstream)
{
	int zerr;

	if ((zerr = deflateInit(zstream, Z_DEFAULT_COMPRESSION)) != Z_OK)
		return zstream_seterr(zerr, zstream);

	return 0;
}

ssize_t git_zstream_deflate(void *out, size_t out_len, git_zstream *zstream, const void *in, size_t in_len)
{
	int zerr;

	if ((ssize_t)out_len < 0)
		out_len = INT_MAX;

	zstream->next_in = (Bytef *)in;
	zstream->avail_in = in_len;
	zstream->next_out = out;
	zstream->avail_out = out_len;

	if ((zerr = deflate(zstream, Z_FINISH)) == Z_STREAM_ERROR)
		return zstream_seterr(zerr, zstream);

	return (out_len - zstream->avail_out);
}

void git_zstream_reset(git_zstream *zstream)
{
	deflateReset(zstream);
}

void git_zstream_free(git_zstream *zstream)
{
	deflateEnd(zstream);
}

int git_zstream_deflatebuf(git_buf *out, const void *in, size_t in_len)
{
	git_zstream zstream = GIT_ZSTREAM_INIT;
	size_t out_len;
	ssize_t written;
	int error = 0;

	if ((error = git_zstream_init(&zstream)) < 0)
		return error;

	do {
		if (out->asize - out->size < BUFFER_SIZE)
			git_buf_grow(out, out->asize + BUFFER_SIZE);

		out_len = out->asize - out->size;

		if ((written = git_zstream_deflate(out->ptr + out->size, out_len, &zstream, in, in_len)) <= 0)
			break;

		in = (char *)in + written;
		in_len -= written;
		out->size += written;
	} while (written > 0);

	if (written < 0)
		error = written;

	git_zstream_free(&zstream);
	return error;
}
