/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifdef GIT_SSH

#include "git2/repository.h"
#include "git2/revwalk.h"

#include "transport.h"
#include "fetch.h"
#include "netops.h"
#include "protocol.h"
#include "pkt.h"
#include "transports/util.h"

typedef struct {
	git_transport parent;

	git_protocol proto;

	git_vector refs;
	git_remote_head **heads;

	git_transport_caps caps;

	char buff[1024];
	gitno_buffer buf;

#ifdef GIT_WIN32
	WSADATA wsd;
#endif
} transport_ssh;

static int gen_request(git_buf *request, const char *cmd, const char *url)
{
	char *delim, *repo;
	char default_command[] = "git-upload-pack";

	delim = strchr(url, '/');
	if (delim == NULL) {
		giterr_set(GITERR_NET, "Malformed URL");
		return -1;
	}

	repo = delim;

	delim = strchr(url, ':');
	if (delim == NULL)
		delim = strchr(url, '/');

	if (cmd == NULL)
		cmd = default_command;

	git_buf_grow(request, strlen(cmd) + strlen(repo) + 2);
	git_buf_printf(request, "%s %s", cmd, repo);
	git_buf_putc(request, '\0');

	if (git_buf_oom(request))
		return -1;

	return 0;
}

static int send_request(git_transport *t, const char *cmd, const char *url)
{
	int error;
	git_buf request = GIT_BUF_INIT;

	error = gen_request(&request, cmd, url);
	if (error < 0)
		goto on_error;

	error = gitno_ssh_exec(t, git_buf_cstr(&request));

on_error:
	git_buf_free(&request);
	return error;
}

static int store_refs(transport_ssh *t)
{
	int ret = 0;
	gitno_buffer *buf = &t->buf;

	while (1) {
		ret = gitno_ssh_recv(&t->parent, buf);
		if (ret < 0)
			return -1;

		if (ret == 0)
			return 0;

		ret = git_protocol_store_refs(&t->proto, buf->data, ret);
		if (ret == GIT_EBUFS) {
			gitno_consume_n(buf, buf->len);
			continue;
		}

		if (ret < 0)
			return ret;

		gitno_consume_n(buf, buf->offset);

		if (t->proto.flush) {
			t->proto.flush = 0;
			return 0;
		}

	}
}

static const char *prefixes[] = {
	"ssh://",
	"ssh+git://",
	"git+ssh://",
	NULL
};

static int do_connect(transport_ssh *t, const char *url)
{
	int i;
	char *host, *port, *at;

	for (i=0; prefixes[i]; i++) {
		if (!git__prefixcmp(url, prefixes[i])) {
			url += strlen(prefixes[i]);
			break;
		}
	}

	/* skip username in URL; user is responsible
	 * for doing authentication */
	at = strchr(url, '@');
	if (at != NULL)
		url = at+1;

	if (gitno_extract_host_and_port(&host, &port, url, "22") < 0)
		return -1;

	if (gitno_ssh_connect(&t->parent, host, port))
		return -1;

	if (send_request((git_transport *) t, NULL, at+1) < 0)
		return -1;

	t->parent.connected = 1;

	gitno_buffer_setup(&t->parent, &t->buf, t->buff, sizeof(t->buff));

	if (store_refs(t) < 0)
		return -1;

	if (detect_caps(&t->caps, &t->refs) < 0)
		return -1;

	git__free(host);
	git__free(port);

	return 0;
}

static int ssh_connect(git_transport *transport, int direction)
{
	transport_ssh *t = (transport_ssh *) transport;

	if (direction == GIT_DIR_PUSH) {
		giterr_set(GITERR_NET, "Pushing over ssh:// is not yet supported");
		return -1;
	}

	t->parent.direction = direction;
	if (git_vector_init(&t->refs, 16, NULL) < 0)
		return -1;

	if (do_connect(t, transport->url) < 0)
		return -1;

	return 0;
}

static int ssh_ls(
	git_transport *transport,
	git_headlist_cb list_cb,
	void *opaque)
{
	transport_ssh *t = (transport_ssh *) transport;
	git_vector *refs = &t->refs;
	unsigned int i;
	git_pkt *p = NULL;

	git_vector_foreach(refs, i, p) {
		git_pkt_ref *pkt = NULL;

		if (p->type != GIT_PKT_REF)
			continue;

		pkt = (git_pkt_ref *)p;

		if (list_cb(&pkt->head, opaque) < 0) {
			giterr_set(GITERR_NET, "User callback returned error");
			return -1;
		}
	}
	return 0;
}

static int recv_pkt(git_transport *t, gitno_buffer *buf)
{
	int ret, pkt_type;
	const char *ptr = buf->data, *line_end;
	git_pkt *pkt;

	do {
		if ((ret = gitno_ssh_recv(t, buf)) < 0)
			return -1;

		ret = git_pkt_parse_line(&pkt, ptr, &line_end, buf->offset);
		if (ret == GIT_EBUFS)
			continue;
		if (ret < 0)
			return -1;
	} while (ret);

	gitno_consume(buf, line_end);
	pkt_type = pkt->type;
	git__free(pkt);

	return pkt_type;
}

static int ssh_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_vector *wants)
{
	int error;
	unsigned int i;
	transport_ssh *t = (transport_ssh *) transport;
	git_revwalk *walk;
	git_oid oid;
	git_buf data = GIT_BUF_INIT;
	gitno_buffer *buf = &t->buf;

	if (git_pkt_buffer_wants(wants, &t->caps, &data) < 0)
		return -1;

	if (git_fetch_setup_walk(&walk, repo) < 0)
		goto on_error;

	if (gitno_ssh_send(&t->parent, data.ptr, data.size) < 0)
		goto on_error;

	git_buf_clear(&data);
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
			int pkt_type;

			git_pkt_buffer_flush(&data);
			if (git_buf_oom(&data))
				goto on_error;

			if (gitno_ssh_send(&t->parent, data.ptr, data.size) < 0)
				goto on_error;

			pkt_type = recv_pkt(&t->parent, buf);

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
	if (error < 0 && error != GIT_REVWALKOVER)
		goto on_error;

	/* Tell the other end that we're done negotiating */
	git_buf_clear(&data);
	git_pkt_buffer_flush(&data);
	git_pkt_buffer_done(&data);
	if (gitno_ssh_send(&t->parent, data.ptr, data.size) < 0)
		goto on_error;

	git_buf_free(&data);
	git_revwalk_free(walk);
	return 0;

on_error:
	git_buf_free(&data);
	git_revwalk_free(walk);
	return -1;
}

static int download_pack(
	git_transport *t,
	const char *buffered,
	size_t buffered_size,
	git_repository *repo,
	git_off_t *bytes,
	git_indexer_stats *stats)
{
	int recvd;
	char buff[1024];
	gitno_buffer buf;
	git_indexer_stream *idx;

	gitno_buffer_setup(t, &buf, buff, sizeof(buff));

	if (memcmp(buffered, "PACK", strlen("PACK"))) {
		giterr_set(GITERR_NET, "The pack doesn't start with the signature");
		return -1;
	}

	if (git_indexer_stream_new(&idx, git_repository_path(repo)) < 0)
		return -1;

	memset(stats, 0, sizeof(git_indexer_stats));
	if (git_indexer_stream_add(idx, buffered, buffered_size, stats) < 0)
		goto on_error;

	*bytes = buffered_size;

	do {
		if (git_indexer_stream_add(idx, buf.data, buf.offset, stats) < 0)
			goto on_error;

		gitno_consume_n(&buf, buf.offset);
		if ((recvd = gitno_ssh_recv(t, &buf)) < 0)
			goto on_error;

		*bytes += recvd;
	} while(recvd > 0);

	if (git_indexer_stream_finalize(idx, stats))
		goto on_error;

	git_indexer_stream_free(idx);
	return 0;

on_error:
	git_indexer_stream_free(idx);
	return -1;
}

static int ssh_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_off_t *bytes,
	git_indexer_stats *stats)
{
	int error = 0, read_bytes;
	const char *line_end, *ptr;
	transport_ssh *t = (transport_ssh *) transport;
	gitno_buffer *buf = &t->buf;
	git_pkt *pkt;

	/*
	 * For now, we ignore everything and wait for the pack
	 */
	do {
		ptr = buf->data;
		/* Whilst we're searching for the pack */
		while (1) {
			if (buf->offset == 0) {
				break;
			}

			error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->offset);
			if (error == GIT_EBUFS)
				break;

			if (error < 0)
				return error;

			if (pkt->type == GIT_PKT_PACK) {
				git__free(pkt);
				return download_pack(transport, buf->data, buf->offset,
						     repo, bytes, stats);
			}

			/* For now we don't care about anything */
			git__free(pkt);
			gitno_consume(buf, line_end);
		}

		read_bytes = gitno_ssh_recv(transport, buf);
	} while (read_bytes);

	return read_bytes;
}

static const char pkt_flush_str[] = "0000";

static int ssh_close(git_transport *transport)
{
	gitno_ssh_send(transport, pkt_flush_str, sizeof(pkt_flush_str));

	if (gitno_ssh_teardown(transport) < 0) {
		giterr_set(GITERR_NET, "Failed to teardown ssh connection");
		return -1;
	}

	if (gitno_close(transport->socket) < 0) {
		giterr_set(GITERR_NET, "Failed to close socket");
		return -1;
	}

#ifdef GIT_WIN32
	WSACleanup();
#endif

	return transport->connected = 0;
}

static void ssh_free(git_transport *transport)
{
	transport_ssh *t = (transport_ssh *) transport;
	git_vector *refs = &t->refs;
	git_pkt *p;
	unsigned int i;

	git_vector_foreach(refs, i, p) {
		git_pkt_free(p);
	}

	git_vector_free(refs);
	git__free(t->heads);
	git_buf_free(&t->proto.buf);
	git__free(t->parent.url);
	git__free(t);
}

int git_transport_ssh(git_transport **out)
{
	transport_ssh *t;
#ifdef GIT_WIN32
	int ret;
#endif

	t = git__malloc(sizeof(transport_ssh));
	GITERR_CHECK_ALLOC(t);

	memset(t, 0x0, sizeof(transport_ssh));

	t->parent.socket = -1;

	t->parent.ssh.session = NULL;
	t->parent.ssh.channel = NULL;

	t->parent.connect = ssh_connect;
	t->parent.ls = ssh_ls;

	// TODO: push

	t->parent.negotiate_fetch = ssh_negotiate_fetch;
	t->parent.download_pack = ssh_download_pack;
	t->parent.close = ssh_close;
	t->parent.free = ssh_free;

	t->proto.refs = &t->refs;
	t->proto.transport = (git_transport *) t;

#ifdef GIT_WIN32
	ret = WSAStartup(MAKEWORD(2,2), &t->wsd);
	if (ret != 0) {
		git_free(t);
		giterr_set(GITERR_NET, "Winsock init failed");
		return -1;
	}
#endif

	*out = (git_transport *) t;
	return 0;
}
#endif /* GIT_SSH */
