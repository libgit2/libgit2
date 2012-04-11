/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/net.h"
#include "git2/common.h"
#include "git2/types.h"
#include "git2/errors.h"
#include "git2/net.h"
#include "git2/revwalk.h"

#include "vector.h"
#include "transport.h"
#include "pkt.h"
#include "common.h"
#include "netops.h"
#include "filebuf.h"
#include "repository.h"
#include "fetch.h"
#include "protocol.h"

typedef struct {
	git_transport parent;
	git_protocol proto;
	GIT_SOCKET socket;
	git_vector refs;
	git_remote_head **heads;
	git_transport_caps caps;
	char buff[1024];
	gitno_buffer buf;
#ifdef GIT_WIN32
	WSADATA wsd;
#endif
} transport_git;

/*
 * Create a git procol request.
 *
 * For example: 0035git-upload-pack /libgit2/libgit2\0host=github.com\0
 */
static int gen_proto(git_buf *request, const char *cmd, const char *url)
{
	char *delim, *repo;
	char default_command[] = "git-upload-pack";
	char host[] = "host=";
	int len;

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

	len = 4 + strlen(cmd) + 1 + strlen(repo) + 1 + strlen(host) + (delim - url) + 1;

	git_buf_grow(request, len);
	git_buf_printf(request, "%04x%s %s%c%s", len, cmd, repo, 0, host);
	git_buf_put(request, url, delim - url);
	git_buf_putc(request, '\0');

	if (git_buf_oom(request))
		return GIT_ENOMEM;

	return 0;
}

static int send_request(GIT_SOCKET s, const char *cmd, const char *url)
{
	int error;
	git_buf request = GIT_BUF_INIT;

	error = gen_proto(&request, cmd, url);
	if (error < 0)
		goto cleanup;

	error = gitno_send(s, request.ptr, request.size, 0);

cleanup:
	git_buf_free(&request);
	return error;
}

/*
 * Parse the URL and connect to a server, storing the socket in
 * out. For convenience this also takes care of asking for the remote
 * refs
 */
static int do_connect(transport_git *t, const char *url)
{
	GIT_SOCKET s;
	char *host, *port;
	const char prefix[] = "git://";
	int error, connected = 0;

	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	if (gitno_extract_host_and_port(&host, &port, url, GIT_DEFAULT_PORT) < 0)
		return -1;

	s = gitno_connect(host, port);
	connected = 1;
	error = send_request(s, NULL, url);
	t->socket = s;

	git__free(host);
	git__free(port);

	if (error < GIT_SUCCESS && s > 0)
		close(s);
	if (!connected) {
		giterr_set(GITERR_NET, "Failed to connect to the host");
		return -1;
	}

	return error;
}

/*
 * Read from the socket and store the references in the vector
 */
static int store_refs(transport_git *t)
{
	gitno_buffer *buf = &t->buf;
	int ret = 0;

	while (1) {
		if ((ret = gitno_recv(buf)) < 0)
			return -1;
		if (ret == 0) /* Orderly shutdown, so exit */
			return 0;

		ret = git_protocol_store_refs(&t->proto, buf->data, buf->offset);
		if (ret == GIT_ESHORTBUFFER) {
			gitno_consume_n(buf, buf->len);
			continue;
		}

		if (ret < 0)
			return ret;

		gitno_consume_n(buf, buf->offset);

		if (t->proto.flush) { /* No more refs */
			t->proto.flush = 0;
			return 0;
		}
	}
}

static int detect_caps(transport_git *t)
{
	git_vector *refs = &t->refs;
	git_pkt_ref *pkt;
	git_transport_caps *caps = &t->caps;
	const char *ptr;

	pkt = git_vector_get(refs, 0);
	/* No refs or capabilites, odd but not a problem */
	if (pkt == NULL || pkt->capabilities == NULL)
		return 0;

	ptr = pkt->capabilities;
	while (ptr != NULL && *ptr != '\0') {
		if (*ptr == ' ')
			ptr++;

		if(!git__prefixcmp(ptr, GIT_CAP_OFS_DELTA)) {
			caps->common = caps->ofs_delta = 1;
			ptr += strlen(GIT_CAP_OFS_DELTA);
			continue;
		}

		/* We don't know this capability, so skip it */
		ptr = strchr(ptr, ' ');
	}

	return 0;
}

/*
 * Since this is a network connection, we need to parse and store the
 * pkt-lines at this stage and keep them there.
 */
static int git_connect(git_transport *transport, int direction)
{
	transport_git *t = (transport_git *) transport;

	if (direction == GIT_DIR_PUSH) {
		giterr_set(GITERR_NET, "Pushing over git:// is not supported");
		return -1;
	}

	t->parent.direction = direction;
	if (git_vector_init(&t->refs, 16, NULL) < 0)
		return -1;

	/* Connect and ask for the refs */
	if (do_connect(t, transport->url) < 0)
		goto cleanup;

	gitno_buffer_setup(&t->buf, t->buff, sizeof(t->buff), t->socket);

	t->parent.connected = 1;
	if (store_refs(t) < 0)
		goto cleanup;

	if (detect_caps(t) < 0)
		goto cleanup;

	return 0;
cleanup:
	git_vector_free(&t->refs);
	return -1;
}

static int git_ls(git_transport *transport, git_headlist_cb list_cb, void *opaque)
{
	transport_git *t = (transport_git *) transport;
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

/* Wait until we get an ack from the */
static int recv_pkt(gitno_buffer *buf)
{
	const char *ptr = buf->data, *line_end;
	git_pkt *pkt;
	int pkt_type, error;

	do {
		/* Wait for max. 1 second */
		if ((error = gitno_select_in(buf, 1, 0)) < 0) {
			return -1;
		} else if (error == 0) {
			/*
			 * Some servers don't respond immediately, so if this
			 * happens, we keep sending information until it
			 * answers. Pretend we received a NAK to convince higher
			 * layers to do so.
			 */
			return GIT_PKT_NAK;
		}

		if ((error = gitno_recv(buf)) < 0)
			return -1;

		error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->offset);
		if (error == GIT_ESHORTBUFFER)
			continue;
		if (error < 0)
			return -1;
	} while (error);

	gitno_consume(buf, line_end);
	pkt_type = pkt->type;
	git__free(pkt);

	return pkt_type;
}

static int git_negotiate_fetch(git_transport *transport, git_repository *repo, const git_vector *wants)
{
	transport_git *t = (transport_git *) transport;
	git_revwalk *walk;
	git_reference *ref;
	git_strarray refs;
	git_oid oid;
	int error;
	unsigned int i;
	gitno_buffer *buf = &t->buf;

	if (git_pkt_send_wants(wants, &t->caps, t->socket) < 0)
		return -1;

	if (git_reference_listall(&refs, repo, GIT_REF_LISTALL) < 0)
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

		if ((error = git_revwalk_push(walk, git_reference_oid(ref))) < 0)
			goto on_error;

	}
	git_strarray_free(&refs);

	/*
	 * We don't support any kind of ACK extensions, so the negotiation
	 * boils down to sending what we have and listening for an ACK
	 * every once in a while.
	 */
	i = 0;
	while ((error = git_revwalk_next(&oid, walk)) == 0) {
		error = git_pkt_send_have(&oid, t->socket);
		i++;
		if (i % 20 == 0) {
			int pkt_type;

			git_pkt_send_flush(t->socket);
			pkt_type = recv_pkt(buf);

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
	if (error != GIT_EREVWALKOVER)
		goto on_error;

	git_pkt_send_flush(t->socket);
	git_pkt_send_done(t->socket);

	git_revwalk_free(walk);
	return 0;

on_error:
	git_revwalk_free(walk);
	return -1;
}

static int git_send_flush(git_transport *transport)
{
	transport_git *t = (transport_git *) transport;

	return git_pkt_send_flush(t->socket);
}

static int git_send_done(git_transport *transport)
{
	transport_git *t = (transport_git *) transport;

	return git_pkt_send_done(t->socket);
}

static int git_download_pack(char **out, git_transport *transport, git_repository *repo)
{
	transport_git *t = (transport_git *) transport;
	int error = 0, read_bytes;
	gitno_buffer *buf = &t->buf;
	git_pkt *pkt;
	const char *line_end, *ptr;

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
			if (error == GIT_ESHORTBUFFER)
				break;

			if (error < GIT_SUCCESS)
				return error;

			if (pkt->type == GIT_PKT_PACK) {
				git__free(pkt);
				return git_fetch__download_pack(out, buf->data, buf->offset, t->socket, repo);
			}

			/* For now we don't care about anything */
			git__free(pkt);
			gitno_consume(buf, line_end);
		}

		read_bytes = gitno_recv(buf);
	} while (read_bytes);

	return read_bytes;
}


static int git_close(git_transport *transport)
{
	transport_git *t = (transport_git*) transport;

	/* Can't do anything if there's an error, so don't bother checking  */
	git_pkt_send_flush(t->socket);
	if (gitno_close(t->socket) < 0) {
		giterr_set(GITERR_NET, "Failed to close socket");
		return -1;
	}

#ifdef GIT_WIN32
	WSACleanup();
#endif

	return 0;
}

static void git_free(git_transport *transport)
{
	transport_git *t = (transport_git *) transport;
	git_vector *refs = &t->refs;
	unsigned int i;

	for (i = 0; i < refs->length; ++i) {
		git_pkt *p = git_vector_get(refs, i);
		git_pkt_free(p);
	}

	git_vector_free(refs);
	git__free(t->heads);
	git_buf_free(&t->proto.buf);
	git__free(t->parent.url);
	git__free(t);
}

int git_transport_git(git_transport **out)
{
	transport_git *t;
#ifdef GIT_WIN32
	int ret;
#endif

	t = git__malloc(sizeof(transport_git));
	GITERR_CHECK_ALLOC(t);

	memset(t, 0x0, sizeof(transport_git));

	t->parent.connect = git_connect;
	t->parent.ls = git_ls;
	t->parent.negotiate_fetch = git_negotiate_fetch;
	t->parent.send_flush = git_send_flush;
	t->parent.send_done = git_send_done;
	t->parent.download_pack = git_download_pack;
	t->parent.close = git_close;
	t->parent.free = git_free;
	t->proto.refs = &t->refs;
	t->proto.transport = (git_transport *) t;

	*out = (git_transport *) t;

#ifdef GIT_WIN32
	ret = WSAStartup(MAKEWORD(2,2), &t->wsd);
	if (ret != 0) {
		git_free(*out);
		giterr_set(GITERR_NET, "Winsock init failed");
		return -1;
	}
#endif

	return 0;
}
