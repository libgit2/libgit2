/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdlib.h>
#include "git2.h"
#include "http_parser.h"

#include "transport.h"
#include "common.h"
#include "netops.h"
#include "buffer.h"
#include "pkt.h"
#include "refs.h"
#include "pack.h"
#include "fetch.h"
#include "filebuf.h"
#include "repository.h"
#include "protocol.h"

enum last_cb {
	NONE,
	FIELD,
	VALUE
};

typedef struct {
	git_transport parent;
	git_protocol proto;
	git_vector refs;
	git_vector common;
	GIT_SOCKET socket;
	git_buf buf;
	git_remote_head **heads;
	int error;
	int transfer_finished :1,
		ct_found :1,
		ct_finished :1,
		pack_ready :1;
	enum last_cb last_cb;
	http_parser parser;
	char *content_type;
	char *host;
	char *port;
	char *service;
	git_transport_caps caps;
#ifdef GIT_WIN32
	WSADATA wsd;
#endif
} transport_http;

static int gen_request(git_buf *buf, const char *url, const char *host, const char *op,
                       const char *service, ssize_t content_length, int ls)
{
	const char *path = url;

	path = strchr(path, '/');
	if (path == NULL) /* Is 'git fetch http://host.com/' valid? */
		path = "/";

	if (ls) {
		git_buf_printf(buf, "%s %s/info/refs?service=git-%s HTTP/1.1\r\n", op, path, service);
	} else {
		git_buf_printf(buf, "%s %s/git-%s HTTP/1.1\r\n", op, path, service);
	}
	git_buf_puts(buf, "User-Agent: git/1.0 (libgit2 " LIBGIT2_VERSION ")\r\n");
	git_buf_printf(buf, "Host: %s\r\n", host);
	if (content_length > 0) {
		git_buf_printf(buf, "Accept: application/x-git-%s-result\r\n", service);
		git_buf_printf(buf, "Content-Type: application/x-git-%s-request\r\n", service);
		git_buf_printf(buf, "Content-Length: %"PRIuZ "\r\n", content_length);
	} else {
		git_buf_puts(buf, "Accept: */*\r\n");
	}
	git_buf_puts(buf, "\r\n");

	if (git_buf_oom(buf))
		return -1;

	return 0;
}

static int do_connect(transport_http *t, const char *host, const char *port)
{
	GIT_SOCKET s;

	if (t->parent.connected && http_should_keep_alive(&t->parser))
		return 0;

	if (gitno_connect(&s, host, port) < 0)
		return -1;

	t->socket = s;
	t->parent.connected = 1;

	return 0;
}

/*
 * The HTTP parser is streaming, so we need to wait until we're in the
 * field handler before we can be sure that we can store the previous
 * value. Right now, we only care about the
 * Content-Type. on_header_{field,value} should be kept generic enough
 * to work for any request.
 */

static const char *typestr = "Content-Type";

static int on_header_field(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;

	if (t->last_cb == VALUE && t->ct_found) {
		t->ct_finished = 1;
		t->ct_found = 0;
		t->content_type = git__strdup(git_buf_cstr(buf));
		GITERR_CHECK_ALLOC(t->content_type);
		git_buf_clear(buf);
	}

	if (t->ct_found) {
		t->last_cb = FIELD;
		return 0;
	}

	if (t->last_cb != FIELD)
		git_buf_clear(buf);

	git_buf_put(buf, str, len);
	t->last_cb = FIELD;

	return git_buf_oom(buf);
}

static int on_header_value(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;

	if (t->ct_finished) {
		t->last_cb = VALUE;
		return 0;
	}

	if (t->last_cb == VALUE)
		git_buf_put(buf, str, len);

	if (t->last_cb == FIELD && !strcmp(git_buf_cstr(buf), typestr)) {
		t->ct_found = 1;
		git_buf_clear(buf);
		git_buf_put(buf, str, len);
	}

	t->last_cb = VALUE;

	return git_buf_oom(buf);
}

static int on_headers_complete(http_parser *parser)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;

	/* The content-type is text/plain for 404, so don't validate */
	if (parser->status_code == 404) {
		git_buf_clear(buf);
		return 0;
	}

	if (t->content_type == NULL) {
		t->content_type = git__strdup(git_buf_cstr(buf));
		if (t->content_type == NULL)
			return t->error = -1;
	}

	git_buf_clear(buf);
	git_buf_printf(buf, "application/x-git-%s-advertisement", t->service);
	if (git_buf_oom(buf))
		return t->error = -1;

	if (strcmp(t->content_type, git_buf_cstr(buf)))
		return t->error = -1;

	git_buf_clear(buf);
	return 0;
}

static int on_body_store_refs(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;

	if (parser->status_code == 404) {
		return git_buf_put(&t->buf, str, len);
	}

	return git_protocol_store_refs(&t->proto, str, len);
}

static int on_message_complete(http_parser *parser)
{
	transport_http *t = (transport_http *) parser->data;

	t->transfer_finished = 1;

	if (parser->status_code == 404) {
		giterr_set(GITERR_NET, "Remote error: %s", git_buf_cstr(&t->buf));
		t->error = -1;
	}

	return 0;
}

static int store_refs(transport_http *t)
{
	http_parser_settings settings;
	char buffer[1024];
	gitno_buffer buf;
	git_pkt *pkt;
	int ret;

	http_parser_init(&t->parser, HTTP_RESPONSE);
	t->parser.data = t;
	memset(&settings, 0x0, sizeof(http_parser_settings));
	settings.on_header_field = on_header_field;
	settings.on_header_value = on_header_value;
	settings.on_headers_complete = on_headers_complete;
	settings.on_body = on_body_store_refs;
	settings.on_message_complete = on_message_complete;

	gitno_buffer_setup(&buf, buffer, sizeof(buffer), t->socket);

	while(1) {
		size_t parsed;

		if ((ret = gitno_recv(&buf)) < 0)
			return -1;

		parsed = http_parser_execute(&t->parser, &settings, buf.data, buf.offset);
		/* Both should happen at the same time */
		if (parsed != buf.offset || t->error < 0)
			return t->error;

		gitno_consume_n(&buf, parsed);

		if (ret == 0 || t->transfer_finished)
			return 0;
	}

	pkt = git_vector_get(&t->refs, 0);
	if (pkt == NULL || pkt->type != GIT_PKT_COMMENT) {
		giterr_set(GITERR_NET, "Invalid HTTP response");
		return t->error = -1;
	} else {
		git_vector_remove(&t->refs, 0);
	}

	return 0;
}

static int http_connect(git_transport *transport, int direction)
{
	transport_http *t = (transport_http *) transport;
	int ret;
	git_buf request = GIT_BUF_INIT;
	const char *service = "upload-pack";
	const char *url = t->parent.url, *prefix = "http://";

	if (direction == GIT_DIR_PUSH) {
		giterr_set(GITERR_NET, "Pushing over HTTP is not implemented");
		return -1;
	}

	t->parent.direction = direction;
	if (git_vector_init(&t->refs, 16, NULL) < 0)
		return -1;

	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	if ((ret = gitno_extract_host_and_port(&t->host, &t->port, url, "80")) < 0)
		goto cleanup;

	t->service = git__strdup(service);
	GITERR_CHECK_ALLOC(t->service);

	if ((ret = do_connect(t, t->host, t->port)) < 0)
		goto cleanup;

	/* Generate and send the HTTP request */
	if ((ret = gen_request(&request, url, t->host, "GET", service, 0, 1)) < 0) {
		giterr_set(GITERR_NET, "Failed to generate request");
		goto cleanup;
	}

	if ((ret = gitno_send(t->socket, request.ptr, request.size, 0)) < 0)
		goto cleanup;

	ret = store_refs(t);

cleanup:
	git_buf_free(&request);
	git_buf_clear(&t->buf);

	return ret;
}

static int http_ls(git_transport *transport, git_headlist_cb list_cb, void *opaque)
{
	transport_http *t = (transport_http *) transport;
	git_vector *refs = &t->refs;
	unsigned int i;
	git_pkt_ref *p;

	git_vector_foreach(refs, i, p) {
		if (p->type != GIT_PKT_REF)
			continue;

		if (list_cb(&p->head, opaque) < 0) {
			giterr_set(GITERR_NET, "The user callback returned error");
			return -1;
		}
	}

	return 0;
}

static int on_body_parse_response(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;
	git_vector *common = &t->common;
	int error;
	const char *line_end, *ptr;

	if (len == 0) { /* EOF */
		if (git_buf_len(buf) != 0) {
			giterr_set(GITERR_NET, "Unexpected EOF");
			return t->error = -1;
		} else {
			return 0;
		}
	}

	git_buf_put(buf, str, len);
	ptr = buf->ptr;
	while (1) {
		git_pkt *pkt;

		if (git_buf_len(buf) == 0)
			return 0;

		error = git_pkt_parse_line(&pkt, ptr, &line_end, git_buf_len(buf));
		if (error == GIT_EBUFS) {
			return 0; /* Ask for more */
		}
		if (error < 0)
			return t->error = -1;

		git_buf_consume(buf, line_end);

		if (pkt->type == GIT_PKT_PACK) {
			git__free(pkt);
			t->pack_ready = 1;
			return 0;
		}

		if (pkt->type == GIT_PKT_NAK) {
			git__free(pkt);
			return 0;
		}

		if (pkt->type != GIT_PKT_ACK) {
			git__free(pkt);
			continue;
		}

		if (git_vector_insert(common, pkt) < 0)
			return -1;
	}

	return error;

}

static int parse_response(transport_http *t)
{
	int ret = 0;
	http_parser_settings settings;
	char buffer[1024];
	gitno_buffer buf;

	http_parser_init(&t->parser, HTTP_RESPONSE);
	t->parser.data = t;
	t->transfer_finished = 0;
	memset(&settings, 0x0, sizeof(http_parser_settings));
	settings.on_header_field = on_header_field;
	settings.on_header_value = on_header_value;
	settings.on_headers_complete = on_headers_complete;
	settings.on_body = on_body_parse_response;
	settings.on_message_complete = on_message_complete;

	gitno_buffer_setup(&buf, buffer, sizeof(buffer), t->socket);

	while(1) {
		size_t parsed;

		if ((ret = gitno_recv(&buf)) < 0)
			return -1;

		parsed = http_parser_execute(&t->parser, &settings, buf.data, buf.offset);
		/* Both should happen at the same time */
		if (parsed != buf.offset || t->error < 0)
			return t->error;

		gitno_consume_n(&buf, parsed);

		if (ret == 0 || t->transfer_finished || t->pack_ready) {
			return 0;
		}
	}

	return ret;
}

static int http_negotiate_fetch(git_transport *transport, git_repository *repo, const git_vector *wants)
{
	transport_http *t = (transport_http *) transport;
	int ret;
	unsigned int i;
	char buff[128];
	gitno_buffer buf;
	git_revwalk *walk = NULL;
	git_oid oid;
	git_pkt_ack *pkt;
	git_vector *common = &t->common;
	const char *prefix = "http://", *url = t->parent.url;
	git_buf request = GIT_BUF_INIT, data = GIT_BUF_INIT;
	gitno_buffer_setup(&buf, buff, sizeof(buff), t->socket);

	/* TODO: Store url in the transport */
	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	if (git_vector_init(common, 16, NULL) < 0)
		return -1;

	if (git_fetch_setup_walk(&walk, repo) < 0)
		return -1;

	do {
		if ((ret = do_connect(t, t->host, t->port)) < 0)
			goto cleanup;

		if ((ret = git_pkt_buffer_wants(wants, &t->caps, &data)) < 0)
			goto cleanup;

		/* We need to send these on each connection */
		git_vector_foreach (common, i, pkt) {
			if ((ret = git_pkt_buffer_have(&pkt->oid, &data)) < 0)
				goto cleanup;
		}

		i = 0;
		while ((i < 20) && ((ret = git_revwalk_next(&oid, walk)) == 0)) {
			if ((ret = git_pkt_buffer_have(&oid, &data)) < 0)
				goto cleanup;

			i++;
		}

		git_pkt_buffer_done(&data);

		if ((ret = gen_request(&request, url, t->host, "POST", "upload-pack", data.size, 0)) < 0)
			goto cleanup;

		if ((ret = gitno_send(t->socket, request.ptr, request.size, 0)) < 0)
			goto cleanup;

		if ((ret = gitno_send(t->socket, data.ptr, data.size, 0)) < 0)
			goto cleanup;

		git_buf_clear(&request);
		git_buf_clear(&data);

		if (ret < 0 || i >= 256)
			break;

		if ((ret = parse_response(t)) < 0)
			goto cleanup;

		if (t->pack_ready) {
			ret = 0;
			goto cleanup;
		}

	} while(1);

cleanup:
	git_buf_free(&request);
	git_buf_free(&data);
	git_revwalk_free(walk);
	return ret;
}

typedef struct {
	git_indexer_stream *idx;
	git_indexer_stats *stats;
	transport_http *transport;
} download_pack_cbdata;

static int on_message_complete_download_pack(http_parser *parser)
{
	download_pack_cbdata *data = (download_pack_cbdata *) parser->data;

	data->transport->transfer_finished = 1;

	return 0;
}
static int on_body_download_pack(http_parser *parser, const char *str, size_t len)
{
	download_pack_cbdata *data = (download_pack_cbdata *) parser->data;
	transport_http *t = data->transport;
	git_indexer_stream *idx = data->idx;
	git_indexer_stats *stats = data->stats;

	return t->error = git_indexer_stream_add(idx, str, len, stats);
}

/*
 * As the server is probably using Transfer-Encoding: chunked, we have
 * to use the HTTP parser to download the pack instead of giving it to
 * the simple downloader. Furthermore, we're using keep-alive
 * connections, so the simple downloader would just hang.
 */
static int http_download_pack(git_transport *transport, git_repository *repo, git_off_t *bytes, git_indexer_stats *stats)
{
	transport_http *t = (transport_http *) transport;
	git_buf *oldbuf = &t->buf;
	int recvd;
	http_parser_settings settings;
	char buffer[1024];
	gitno_buffer buf;
	git_indexer_stream *idx = NULL;
	download_pack_cbdata data;

	gitno_buffer_setup(&buf, buffer, sizeof(buffer), t->socket);

	if (memcmp(oldbuf->ptr, "PACK", strlen("PACK"))) {
		giterr_set(GITERR_NET, "The pack doesn't start with a pack signature");
		return -1;
	}

	if (git_indexer_stream_new(&idx, git_repository_path(repo)) < 0)
		return -1;


	/*
	 * This is part of the previous response, so we don't want to
	 * re-init the parser, just set these two callbacks.
	 */
	memset(stats, 0, sizeof(git_indexer_stats));
	data.stats = stats;
	data.idx = idx;
	data.transport = t;
	t->parser.data = &data;
	t->transfer_finished = 0;
	memset(&settings, 0x0, sizeof(settings));
	settings.on_message_complete = on_message_complete_download_pack;
	settings.on_body = on_body_download_pack;
	*bytes = git_buf_len(oldbuf);

	if (git_indexer_stream_add(idx, git_buf_cstr(oldbuf), git_buf_len(oldbuf), stats) < 0)
		goto on_error;

	do {
		size_t parsed;

		if ((recvd = gitno_recv(&buf)) < 0)
			goto on_error;

		parsed = http_parser_execute(&t->parser, &settings, buf.data, buf.offset);
		if (parsed != buf.offset || t->error < 0)
			goto on_error;

		*bytes += recvd;
		gitno_consume_n(&buf, parsed);
	} while (recvd > 0 && !t->transfer_finished);

	if (git_indexer_stream_finalize(idx, stats) < 0)
		goto on_error;

	git_indexer_stream_free(idx);
	return 0;

on_error:
	git_indexer_stream_free(idx);
	return -1;
}

static int http_close(git_transport *transport)
{
	transport_http *t = (transport_http *) transport;

	if (gitno_close(t->socket) < 0) {
		giterr_set(GITERR_OS, "Failed to close the socket: %s", strerror(errno));
		return -1;
	}

	return 0;
}


static void http_free(git_transport *transport)
{
	transport_http *t = (transport_http *) transport;
	git_vector *refs = &t->refs;
	git_vector *common = &t->common;
	unsigned int i;
	git_pkt *p;

#ifdef GIT_WIN32
	/* cleanup the WSA context. note that this context
	 * can be initialized more than once with WSAStartup(),
	 * and needs to be cleaned one time for each init call
	 */
	WSACleanup();
#endif

	git_vector_foreach(refs, i, p) {
		git_pkt_free(p);
	}
	git_vector_free(refs);
	git_vector_foreach(common, i, p) {
		git_pkt_free(p);
	}
	git_vector_free(common);
	git_buf_free(&t->buf);
	git_buf_free(&t->proto.buf);
	git__free(t->heads);
	git__free(t->content_type);
	git__free(t->host);
	git__free(t->port);
	git__free(t->service);
	git__free(t->parent.url);
	git__free(t);
}

int git_transport_http(git_transport **out)
{
	transport_http *t;

	t = git__malloc(sizeof(transport_http));
	GITERR_CHECK_ALLOC(t);

	memset(t, 0x0, sizeof(transport_http));

	t->parent.connect = http_connect;
	t->parent.ls = http_ls;
	t->parent.negotiate_fetch = http_negotiate_fetch;
	t->parent.download_pack = http_download_pack;
	t->parent.close = http_close;
	t->parent.free = http_free;
	t->proto.refs = &t->refs;
	t->proto.transport = (git_transport *) t;

#ifdef GIT_WIN32
	/* on win32, the WSA context needs to be initialized
	 * before any socket calls can be performed */
	if (WSAStartup(MAKEWORD(2,2), &t->wsd) != 0) {
		http_free((git_transport *) t);
		giterr_set(GITERR_OS, "Winsock init failed");
		return -1;
	}
#endif

	*out = (git_transport *) t;
	return 0;
}
