/*
 * Copyright (C) 2009-2011 the libgit2 contributors
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

enum last_cb {
	NONE,
	FIELD,
	VALUE
};

typedef struct {
	git_transport parent;
	git_vector refs;
	git_vector common;
	int socket;
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
		git_buf_printf(buf, "Content-Length: %zd\r\n", content_length);
	} else {
		git_buf_puts(buf, "Accept: */*\r\n");
	}
	git_buf_puts(buf, "\r\n");

	if (git_buf_oom(buf))
		return GIT_ENOMEM;

	return GIT_SUCCESS;
}

static int do_connect(transport_http *t, const char *host, const char *port)
{
	GIT_SOCKET s = -1;

	if (t->parent.connected && http_should_keep_alive(&t->parser))
		return GIT_SUCCESS;

	s = gitno_connect(host, port);
	if (s < GIT_SUCCESS) {
	    return git__rethrow(s, "Failed to connect to host");
	}
	t->socket = s;
	t->parent.connected = 1;

	return GIT_SUCCESS;
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
		if (t->content_type == NULL)
			return t->error = GIT_ENOMEM;
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

	if (t->content_type == NULL) {
		t->content_type = git__strdup(git_buf_cstr(buf));
		if (t->content_type == NULL)
			return t->error = GIT_ENOMEM;
	}

	git_buf_clear(buf);
	git_buf_printf(buf, "application/x-git-%s-advertisement", t->service);
	if (git_buf_oom(buf))
		return GIT_ENOMEM;

	if (strcmp(t->content_type, git_buf_cstr(buf)))
		return t->error = git__throw(GIT_EOBJCORRUPTED, "Content-Type '%s' is wrong", t->content_type);

	git_buf_clear(buf);
	return 0;
}

static int on_body_store_refs(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;
	git_vector *refs = &t->refs;
	int error;
	const char *line_end, *ptr;
	static int first_pkt = 1;

	if (len == 0) { /* EOF */
		if (buf->size != 0)
			return t->error = git__throw(GIT_ERROR, "EOF and unprocessed data");
		else
			return 0;
	}

	git_buf_put(buf, str, len);
	ptr = buf->ptr;
	while (1) {
		git_pkt *pkt;

		if (buf->size == 0)
			return 0;

		error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->size);
		if (error == GIT_ESHORTBUFFER)
			return 0; /* Ask for more */
		if (error < GIT_SUCCESS)
			return t->error = git__rethrow(error, "Failed to parse pkt-line");

		git_buf_consume(buf, line_end);

		if (first_pkt) {
			first_pkt = 0;
			if (pkt->type != GIT_PKT_COMMENT)
				return t->error = git__throw(GIT_EOBJCORRUPTED, "Not a valid smart HTTP response");
		}

		error = git_vector_insert(refs, pkt);
		if (error < GIT_SUCCESS)
			return t->error = git__rethrow(error, "Failed to add pkt to list");
	}

	return error;
}

static int on_message_complete(http_parser *parser)
{
	transport_http *t = (transport_http *) parser->data;

	t->transfer_finished = 1;
	return 0;
}

static int store_refs(transport_http *t)
{
	int error = GIT_SUCCESS;
	http_parser_settings settings;
	char buffer[1024];
	gitno_buffer buf;

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

		error = gitno_recv(&buf);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Error receiving data from network");

		parsed = http_parser_execute(&t->parser, &settings, buf.data, buf.offset);
		/* Both should happen at the same time */
		if (parsed != buf.offset || t->error < GIT_SUCCESS)
			return git__rethrow(t->error, "Error parsing HTTP data");

		gitno_consume_n(&buf, parsed);

		if (error == 0 || t->transfer_finished)
			return GIT_SUCCESS;
	}

	return error;
}

static int http_connect(git_transport *transport, int direction)
{
	transport_http *t = (transport_http *) transport;
	int error;
	git_buf request = GIT_BUF_INIT;
	const char *service = "upload-pack";
	const char *url = t->parent.url, *prefix = "http://";

	if (direction == GIT_DIR_PUSH)
		return git__throw(GIT_EINVALIDARGS, "Pushing over HTTP is not supported");

	t->parent.direction = direction;
	error = git_vector_init(&t->refs, 16, NULL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to init refs vector");

	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	error = gitno_extract_host_and_port(&t->host, &t->port, url, "80");
	if (error < GIT_SUCCESS)
		goto cleanup;

	t->service = git__strdup(service);
	if (t->service == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = do_connect(t, t->host, t->port);
	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to connect to host");
		goto cleanup;
	}

	/* Generate and send the HTTP request */
	error = gen_request(&request, url, t->host, "GET", service, 0, 1);
	if (error < GIT_SUCCESS) {
		error = git__throw(error, "Failed to generate request");
		goto cleanup;
	}

	error = gitno_send(t->socket, request.ptr, request.size, 0);
	if (error < GIT_SUCCESS)
		error = git__rethrow(error, "Failed to send the HTTP request");

	error = store_refs(t);

cleanup:
	git_buf_free(&request);
	git_buf_clear(&t->buf);

	return error;
}

static int http_ls(git_transport *transport, git_headarray *array)
{
	transport_http *t = (transport_http *) transport;
	git_vector *refs = &t->refs;
	unsigned int i;
	int len = 0;
	git_pkt_ref *p;

	array->heads = git__calloc(refs->length, sizeof(git_remote_head*));
	if (array->heads == NULL)
		return GIT_ENOMEM;

	git_vector_foreach(refs, i, p) {
		if (p->type != GIT_PKT_REF)
			continue;

		array->heads[len] = &p->head;
		len++;
	}

	array->len = len;
	t->heads = array->heads;

	return GIT_SUCCESS;
}

static int on_body_parse_response(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;
	git_vector *common = &t->common;
	int error;
	const char *line_end, *ptr;

	if (len == 0) { /* EOF */
		if (buf->size != 0)
			return t->error = git__throw(GIT_ERROR, "EOF and unprocessed data");
		else
			return 0;
	}

	git_buf_put(buf, str, len);
	ptr = buf->ptr;
	while (1) {
		git_pkt *pkt;

		if (buf->size == 0)
			return 0;

		error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->size);
		if (error == GIT_ESHORTBUFFER) {
			return 0; /* Ask for more */
		}
		if (error < GIT_SUCCESS)
			return t->error = git__rethrow(error, "Failed to parse pkt-line");

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

		error = git_vector_insert(common, pkt);
		if (error < GIT_SUCCESS)
			return t->error = git__rethrow(error, "Failed to add pkt to list");
	}

	return error;

}

static int parse_response(transport_http *t)
{
	int error = GIT_SUCCESS;
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

		error = gitno_recv(&buf);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Error receiving data from network");

		parsed = http_parser_execute(&t->parser, &settings, buf.data, buf.offset);
		/* Both should happen at the same time */
		if (parsed != buf.offset || t->error < GIT_SUCCESS)
			return git__rethrow(t->error, "Error parsing HTTP data");

		gitno_consume_n(&buf, parsed);

		if (error == 0 || t->transfer_finished || t->pack_ready) {
			return GIT_SUCCESS;
		}
	}

	return error;
}

static int setup_walk(git_revwalk **out, git_repository *repo)
{
	git_revwalk *walk;
	git_strarray refs;
	unsigned int i;
	git_reference *ref;
	int error;

	error = git_reference_listall(&refs, repo, GIT_REF_LISTALL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to list references");

	error = git_revwalk_new(&walk, repo);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to setup walk");

	git_revwalk_sorting(walk, GIT_SORT_TIME);

	for (i = 0; i < refs.count; ++i) {
		/* No tags */
		if (!git__prefixcmp(refs.strings[i], GIT_REFS_TAGS_DIR))
			continue;

		error = git_reference_lookup(&ref, repo, refs.strings[i]);
		if (error < GIT_ERROR) {
			error = git__rethrow(error, "Failed to lookup %s", refs.strings[i]);
			goto cleanup;
		}

		if (git_reference_type(ref) == GIT_REF_SYMBOLIC)
			continue;
		error = git_revwalk_push(walk, git_reference_oid(ref));
		if (error < GIT_ERROR) {
			error = git__rethrow(error, "Failed to push %s", refs.strings[i]);
			goto cleanup;
		}
	}

	*out = walk;
cleanup:
	git_strarray_free(&refs);

	return error;
}

static int http_negotiate_fetch(git_transport *transport, git_repository *repo, git_headarray *wants)
{
	transport_http *t = (transport_http *) transport;
	int error;
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

	error = git_vector_init(common, 16, NULL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to init common vector");

	error = setup_walk(&walk, repo);
	if (error < GIT_SUCCESS) {
		error =  git__rethrow(error, "Failed to setup walk");
		goto cleanup;
	}

	do {
		error = do_connect(t, t->host, t->port);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to connect to host");
			goto cleanup;
		}

		error =  git_pkt_buffer_wants(wants, &t->caps, &data);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to send wants");
			goto cleanup;
		}

		/* We need to send these on each connection */
		git_vector_foreach (common, i, pkt) {
			error = git_pkt_buffer_have(&pkt->oid, &data);
			if (error < GIT_SUCCESS) {
				error = git__rethrow(error, "Failed to buffer common have");
				goto cleanup;
			}
		}

		i = 0;
		while ((i < 20) && ((error = git_revwalk_next(&oid, walk)) == GIT_SUCCESS)) {
			error = git_pkt_buffer_have(&oid, &data);
			if (error < GIT_SUCCESS) {
				error = git__rethrow(error, "Failed to buffer have");
				goto cleanup;
			}
			i++;
		}

		git_pkt_buffer_done(&data);

		error = gen_request(&request, url, t->host, "POST", "upload-pack", data.size, 0);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to generate request");
			goto cleanup;
		}

		error =  gitno_send(t->socket, request.ptr, request.size, 0);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to send request");
			goto cleanup;
		}

		error =  gitno_send(t->socket, data.ptr, data.size, 0);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to send data");
			goto cleanup;
		}

		git_buf_clear(&request);
		git_buf_clear(&data);

		if (error < GIT_SUCCESS || i >= 256)
			break;

		error = parse_response(t);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Error parsing the response");
			goto cleanup;
		}

		if (t->pack_ready) {
			error = GIT_SUCCESS;
			goto cleanup;
		}

	} while(1);

cleanup:
	git_buf_free(&request);
	git_buf_free(&data);
	git_revwalk_free(walk);
	return error;
}

typedef struct {
	git_filebuf *file;
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
	git_filebuf *file = data->file;


	return t->error = git_filebuf_write(file, str, len);
}

/*
 * As the server is probably using Transfer-Encoding: chunked, we have
 * to use the HTTP parser to download the pack instead of giving it to
 * the simple downloader. Furthermore, we're using keep-alive
 * connections, so the simple downloader would just hang.
 */
static int http_download_pack(char **out, git_transport *transport, git_repository *repo)
{
	transport_http *t = (transport_http *) transport;
	git_buf *oldbuf = &t->buf;
	int error = GIT_SUCCESS;
	http_parser_settings settings;
	char buffer[1024];
	gitno_buffer buf;
	download_pack_cbdata data;
	git_filebuf file;
	char path[GIT_PATH_MAX], suff[] = "/objects/pack/pack-received\0";

	/*
	 * This is part of the previous response, so we don't want to
	 * re-init the parser, just set these two callbacks.
	 */
	data.file = &file;
	data.transport = t;
	t->parser.data = &data;
	t->transfer_finished = 0;
	memset(&settings, 0x0, sizeof(settings));
	settings.on_message_complete = on_message_complete_download_pack;
	settings.on_body = on_body_download_pack;

	gitno_buffer_setup(&buf, buffer, sizeof(buffer), t->socket);

	git_path_join(path, repo->path_repository, suff);

	if (memcmp(oldbuf->ptr, "PACK", strlen("PACK"))) {
		return git__throw(GIT_ERROR, "The pack doesn't start with the signature");
	}

	error = git_filebuf_open(&file, path, GIT_FILEBUF_TEMPORARY);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* Part of the packfile has been received, don't loose it */
	error = git_filebuf_write(&file, oldbuf->ptr, oldbuf->size);
	if (error < GIT_SUCCESS)
		goto cleanup;

	while(1) {
		size_t parsed;

		error = gitno_recv(&buf);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Error receiving data from network");

		parsed = http_parser_execute(&t->parser, &settings, buf.data, buf.offset);
		/* Both should happen at the same time */
		if (parsed != buf.offset || t->error < GIT_SUCCESS)
			return git__rethrow(t->error, "Error parsing HTTP data");

		gitno_consume_n(&buf, parsed);

		if (error == 0 || t->transfer_finished) {
			break;
		}
	}

	*out = git__strdup(file.path_lock);
	if (*out == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	/* A bit dodgy, but we need to keep the pack at the temporary path */
	error = git_filebuf_commit_at(&file, file.path_lock, GIT_PACK_FILE_MODE);

cleanup:
	if (error < GIT_SUCCESS)
		git_filebuf_cleanup(&file);

	return error;
}

static int http_close(git_transport *transport)
{
	transport_http *t = (transport_http *) transport;
	int error;

	error = gitno_close(t->socket);
	if (error < 0)
		return git__throw(GIT_EOSERR, "Failed to close the socket: %s", strerror(errno));

	return GIT_SUCCESS;
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
	if (t == NULL)
		return GIT_ENOMEM;

	memset(t, 0x0, sizeof(transport_http));

	t->parent.connect = http_connect;
	t->parent.ls = http_ls;
	t->parent.negotiate_fetch = http_negotiate_fetch;
	t->parent.download_pack = http_download_pack;
	t->parent.close = http_close;
	t->parent.free = http_free;

#ifdef GIT_WIN32
	/* on win32, the WSA context needs to be initialized
	 * before any socket calls can be performed */
	if (WSAStartup(MAKEWORD(2,2), &t->wsd) != 0) {
		http_free((git_transport *) t);
		return git__throw(GIT_EOSERR, "Winsock init failed");
	}
#endif

	*out = (git_transport *) t;
	return GIT_SUCCESS;
}
