/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "transports/util.h"
#include "pkt.h"

int detect_caps(git_transport_caps *caps, git_vector *refs)
{
	git_pkt_ref *pkt;
	const char *ptr;

	pkt = git_vector_get(refs, 0);
	/* No refs or capabilites, odd but not a problem */
	if (pkt == NULL || pkt->capabilities == NULL)
		return 0;

	ptr = pkt->capabilities;
	while (ptr != NULL && *ptr != '\0') {
		if (*ptr == ' ')
			ptr++;

		if (!git__prefixcmp(ptr, GIT_CAP_OFS_DELTA)) {
			caps->common = caps->ofs_delta = 1;
			ptr += strlen(GIT_CAP_OFS_DELTA);
			continue;
		}

		/* We don't know this capability, so skip it */
		ptr = strchr(ptr, ' ');
	}

	return 0;
}

int store_refs(git_protocol *proto, gitno_buffer *buf)
{
	int ret = 0;

	while (1) {
		if ((ret = gitno_recv(buf)) < 0)
			return -1;
		if (ret == 0) /* orderly shutdown, so exit */
			return 0;

		ret = git_protocol_store_refs(proto,
					      buf->data, buf->offset);
		if (ret == GIT_EBUFS) {
			gitno_consume_n(buf, buf->len);
			continue;
		}
		if (ret < 0)
			return ret;

		gitno_consume_n(buf, buf->offset);

		if (proto->flush) { /* no more refs */
			proto->flush = 0;
			return 0;
		}
	}
}
