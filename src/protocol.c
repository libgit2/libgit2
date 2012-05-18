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
		if (git_buf_len(buf) != 0) {
			giterr_set(GITERR_NET, "Unexpected EOF");
			return p->error = -1;
		} else {
			return 0;
		}
	}

	git_buf_put(buf, data, len);
	ptr = buf->ptr;
	while (1) {
		git_pkt *pkt;

		if (git_buf_len(buf) == 0)
			return 0;

		error = git_pkt_parse_line(&pkt, ptr, &line_end, git_buf_len(buf));
		if (error == GIT_EBUFS)
			return 0; /* Ask for more */
		if (error < 0)
			return p->error = -1;

		git_buf_consume(buf, line_end);

		if (pkt->type == GIT_PKT_ERR) {
			giterr_set(GITERR_NET, "Remote error: %s", ((git_pkt_err *)pkt)->error);
			git__free(pkt);
			return -1;
		}

		if (git_vector_insert(refs, pkt) < 0)
			return p->error = -1;

		if (pkt->type == GIT_PKT_FLUSH)
			p->flush = 1;
	}

	return 0;
}
