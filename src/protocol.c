/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "protocol.h"
#include "pkt.h"
#include "buffer.h"

int git_protocol_store_refs(git_protocol *p, const char *data, size_t len)
{
	git_buf *buf = &p->buf;
	git_vector *refs = p->refs;
	int error;
	const char *line_end, *ptr;

	if (len == 0) { /* EOF */
		if (buf->size != 0)
			return p->error = git__throw(GIT_ERROR, "EOF and unprocessed data");
		else
			return 0;
	}

	git_buf_put(buf, data, len);
	ptr = buf->ptr;
	while (1) {
		git_pkt *pkt;

		if (buf->size == 0)
			return 0;

		error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->size);
		if (error == GIT_ESHORTBUFFER)
			return 0; /* Ask for more */
		if (error < GIT_SUCCESS)
			return p->error = git__rethrow(error, "Failed to parse pkt-line");

		git_buf_consume(buf, line_end);
		error = git_vector_insert(refs, pkt);
		if (error < GIT_SUCCESS)
			return p->error = git__rethrow(error, "Failed to add pkt to list");

		if (pkt->type == GIT_PKT_FLUSH)
			p->flush = 1;
	}

	return error;
}
