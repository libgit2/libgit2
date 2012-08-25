/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/oid.h"
#include "git2/refs.h"
#include "git2/revwalk.h"
#include "git2/indexer.h"

#include "common.h"
#include "transport.h"
#include "remote.h"
#include "refspec.h"
#include "pack.h"
#include "fetch.h"
#include "netops.h"
#include "pkt.h"

struct filter_payload {
	git_remote *remote;
	const git_refspec *spec;
	git_odb *odb;
	int found_head;
};

static int filter_ref__cb(git_remote_head *head, void *payload)
{
	struct filter_payload *p = payload;

	if (!p->found_head && strcmp(head->name, GIT_HEAD_FILE) == 0) {
		p->found_head = 1;
	} else {
		/* If it doesn't match the refpec, we don't want it */
		if (!git_refspec_src_matches(p->spec, head->name))
			return 0;

		/* Don't even try to ask for the annotation target */
		if (!git__suffixcmp(head->name, "^{}"))
			return 0;
	}

	/* If we have the object, mark it so we don't ask for it */
	if (git_odb_exists(p->odb, &head->oid))
		head->local = 1;
	else
		p->remote->need_pack = 1;

	return git_vector_insert(&p->remote->refs, head);
}

static int filter_wants(git_remote *remote)
{
	struct filter_payload p;

	git_vector_clear(&remote->refs);

	/*
	 * The fetch refspec can be NULL, and what this means is that the
	 * user didn't specify one. This is fine, as it means that we're
	 * not interested in any particular branch but just the remote's
	 * HEAD, which will be stored in FETCH_HEAD after the fetch.
	 */
	p.spec = git_remote_fetchspec(remote);
	p.found_head = 0;
	p.remote = remote;

	if (git_repository_odb__weakptr(&p.odb, remote->repo) < 0)
		return -1;

	return git_remote_ls(remote, filter_ref__cb, &p);
}

/* Wait until we get an ack from the */
static int recv_pkt(git_pkt **out, gitno_buffer *buf)
{
	const char *ptr = buf->data, *line_end = ptr;
	git_pkt *pkt;
	int pkt_type, error = 0, ret;

	do {
		if (buf->offset > 0)
			error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->offset);
		else
			error = GIT_EBUFS;

		if (error == 0)
			break; /* return the pkt */

		if (error < 0 && error != GIT_EBUFS)
			return -1;

		if ((ret = gitno_recv(buf)) < 0)
			return -1;
	} while (error);

	gitno_consume(buf, line_end);
	pkt_type = pkt->type;
	if (out != NULL)
		*out = pkt;
	else
		git__free(pkt);

	return pkt_type;
}

static int store_common(git_transport *t)
{
	git_pkt *pkt = NULL;
	gitno_buffer *buf = &t->buffer;

	do {
		if (recv_pkt(&pkt, buf) < 0)
			return -1;

		if (pkt->type == GIT_PKT_ACK) {
			if (git_vector_insert(&t->common, pkt) < 0)
				return -1;
		} else {
			git__free(pkt);
			return 0;
		}

	} while (1);

	return 0;
}

/*
 * In this first version, we push all our refs in and start sending
 * them out. When we get an ACK we hide that commit and continue
 * traversing until we're done
 */
int git_fetch_negotiate(git_remote *remote)
{
	git_transport *t = remote->transport;
	gitno_buffer *buf = &t->buffer;
	git_buf data = GIT_BUF_INIT;
	git_revwalk *walk = NULL;
	int error, pkt_type;
	unsigned int i;
	git_oid oid;

	if (filter_wants(remote) < 0) {
		giterr_set(GITERR_NET, "Failed to filter the reference list for wants");
		return -1;
	}

	/* Don't try to negotiate when we don't want anything */
	if (remote->refs.length == 0 || !remote->need_pack)
		return 0;

	/*
	 * Now we have everything set up so we can start tell the
	 * server what we want and what we have. Call the function if
	 * the transport has its own logic. This is transitional and
	 * will be removed once this function can support git and http.
	 */
	if (t->own_logic)
		return t->negotiate_fetch(t, remote->repo, &remote->refs);

	/* No own logic, do our thing */
	if (git_pkt_buffer_wants(&remote->refs, &t->caps, &data) < 0)
		return -1;

	if (git_fetch_setup_walk(&walk, remote->repo) < 0)
		goto on_error;
	/*
	 * We don't support any kind of ACK extensions, so the negotiation
	 * boils down to sending what we have and listening for an ACK
	 * every once in a while.
	 */
	i = 0;
	while ((error = git_revwalk_next(&oid, walk)) == 0) {
		git_pkt_buffer_have(&oid, &data);
		i++;
		if (i % 20 == 0) {
			git_pkt_buffer_flush(&data);
			if (git_buf_oom(&data))
				goto on_error;

			if (t->negotiation_step(t, data.ptr, data.size) < 0)
				goto on_error;

			git_buf_clear(&data);
			if (t->caps.multi_ack) {
				if (store_common(t) < 0)
					goto on_error;
			} else {
				pkt_type = recv_pkt(NULL, buf);

				if (pkt_type == GIT_PKT_ACK) {
					break;
				} else if (pkt_type == GIT_PKT_NAK) {
					continue;
				} else {
					giterr_set(GITERR_NET, "Unexpected pkt type");
					goto on_error;
				}
			}
		}

		if (t->common.length > 0)
			break;

		if (i % 20 == 0 && t->rpc) {
			git_pkt_ack *pkt;
			unsigned int i;

			if (git_pkt_buffer_wants(&remote->refs, &t->caps, &data) < 0)
				goto on_error;

			git_vector_foreach(&t->common, i, pkt) {
				git_pkt_buffer_have(&pkt->oid, &data);
			}

			if (git_buf_oom(&data))
				goto on_error;
		}
	}

	if (error < 0 && error != GIT_REVWALKOVER)
		goto on_error;

	/* Tell the other end that we're done negotiating */
	if (t->rpc && t->common.length > 0) {
		git_pkt_ack *pkt;
		unsigned int i;

		if (git_pkt_buffer_wants(&remote->refs, &t->caps, &data) < 0)
			goto on_error;

		git_vector_foreach(&t->common, i, pkt) {
			git_pkt_buffer_have(&pkt->oid, &data);
		}

		if (git_buf_oom(&data))
			goto on_error;
	}

	git_pkt_buffer_done(&data);
	if (t->negotiation_step(t, data.ptr, data.size) < 0)
		goto on_error;

	git_buf_free(&data);
	git_revwalk_free(walk);

	/* Now let's eat up whatever the server gives us */
	if (!t->caps.multi_ack) {
		pkt_type = recv_pkt(NULL, buf);
		if (pkt_type != GIT_PKT_ACK && pkt_type != GIT_PKT_NAK) {
			giterr_set(GITERR_NET, "Unexpected pkt type");
			return -1;
		}
	} else {
		git_pkt_ack *pkt;
		do {
			if (recv_pkt((git_pkt **)&pkt, buf) < 0)
				return -1;

			if (pkt->type == GIT_PKT_NAK ||
			    (pkt->type == GIT_PKT_ACK && pkt->status != GIT_ACK_CONTINUE)) {
				git__free(pkt);
				break;
			}

			git__free(pkt);
		} while (1);
	}

	return 0;

on_error:
	git_revwalk_free(walk);
	git_buf_free(&data);
	return -1;
}

int git_fetch_download_pack(git_remote *remote, git_off_t *bytes, git_indexer_stats *stats)
{
	git_transport *t = remote->transport;

	if(!remote->need_pack)
		return 0;

	if (t->own_logic)
		return t->download_pack(t, remote->repo, bytes, stats);

	return git_fetch__download_pack(t, remote->repo, bytes, stats);

}

static int no_sideband(git_indexer_stream *idx, gitno_buffer *buf, git_off_t *bytes, git_indexer_stats *stats)
{
	int recvd;

	do {
		if (git_indexer_stream_add(idx, buf->data, buf->offset, stats) < 0)
			return -1;

		gitno_consume_n(buf, buf->offset);

		if ((recvd = gitno_recv(buf)) < 0)
			return -1;

		*bytes += recvd;
	} while(recvd > 0);

	if (git_indexer_stream_finalize(idx, stats))
		return -1;

	return 0;
}

/* Receiving data from a socket and storing it is pretty much the same for git and HTTP */
int git_fetch__download_pack(
	git_transport *t,
	git_repository *repo,
	git_off_t *bytes,
	git_indexer_stats *stats)
{
	git_buf path = GIT_BUF_INIT;
	gitno_buffer *buf = &t->buffer;
	git_indexer_stream *idx = NULL;

	if (git_buf_joinpath(&path, git_repository_path(repo), "objects/pack") < 0)
		return -1;

	if (git_indexer_stream_new(&idx, git_buf_cstr(&path)) < 0)
		goto on_error;

	git_buf_free(&path);
	memset(stats, 0, sizeof(git_indexer_stats));
	*bytes = 0;

	/*
	 * If the remote doesn't support the side-band, we can feed
	 * the data directly to the indexer. Otherwise, we need to
	 * check which one belongs there.
	 */
	if (!t->caps.side_band && !t->caps.side_band_64k) {
		if (no_sideband(idx, buf, bytes, stats) < 0)
			goto on_error;

		git_indexer_stream_free(idx);
		return 0;
	}

	do {
		git_pkt *pkt;
		if (recv_pkt(&pkt, buf) < 0)
			goto on_error;

		if (pkt->type == GIT_PKT_PROGRESS) {
			if (t->progress_cb) {
				git_pkt_progress *p = (git_pkt_progress *) pkt;
				t->progress_cb(p->data, p->len, t->cb_data);
			}
			git__free(pkt);
		} else if (pkt->type == GIT_PKT_DATA) {
			git_pkt_data *p = (git_pkt_data *) pkt;
			*bytes += p->len;
			if (git_indexer_stream_add(idx, p->data, p->len, stats) < 0)
				goto on_error;

			git__free(pkt);
		} else if (pkt->type == GIT_PKT_FLUSH) {
			/* A flush indicates the end of the packfile */
			git__free(pkt);
			break;
		}
	} while (1);

	if (git_indexer_stream_finalize(idx, stats) < 0)
		goto on_error;

	git_indexer_stream_free(idx);
	return 0;

on_error:
	git_buf_free(&path);
	git_indexer_stream_free(idx);
	return -1;
}

int git_fetch_setup_walk(git_revwalk **out, git_repository *repo)
{
	git_revwalk *walk;
	git_strarray refs;
	unsigned int i;
	git_reference *ref;

	if (git_reference_list(&refs, repo, GIT_REF_LISTALL) < 0)
		return -1;

	if (git_revwalk_new(&walk, repo) < 0)
		return -1;

	git_revwalk_sorting(walk, GIT_SORT_TIME);

	for (i = 0; i < refs.count; ++i) {
		/* No tags */
		if (!git__prefixcmp(refs.strings[i], GIT_REFS_TAGS_DIR))
			continue;

		if (git_reference_lookup(&ref, repo, refs.strings[i]) < 0)
			goto on_error;

		if (git_reference_type(ref) == GIT_REF_SYMBOLIC)
			continue;
		if (git_revwalk_push(walk, git_reference_oid(ref)) < 0)
			goto on_error;

		git_reference_free(ref);
	}

	git_strarray_free(&refs);
	*out = walk;
	return 0;

on_error:
	git_reference_free(ref);
	git_strarray_free(&refs);
	return -1;
}
