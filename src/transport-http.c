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

enum last_cb {
	NONE,
	FIELD,
	VALUE
};

typedef struct {
	git_transport parent;
	git_vector refs;
	int socket;
	git_buf buf;
	git_remote_head **heads;
	int error;
	int transfer_finished :1,
		ct_found :1,
		ct_finished :1;
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

static int gen_request(git_buf *buf, const char *url, const char *host, const char *op, const char *service)
{
	const char *path = url;

	path = strchr(path, '/');
	if (path == NULL) /* Is 'git fetch http://host.com/' valid? */
		path = "/";

	git_buf_printf(buf, "%s %s/info/refs?service=git-%s HTTP/1.1\r\n", op, path, service);
	git_buf_puts(buf, "User-Agent: git/1.0 (libgit2 " LIBGIT2_VERSION ")\r\n");
	git_buf_printf(buf, "Host: %s\r\n", host);
	git_buf_puts(buf, "Accept: */*\r\n" "Pragma: no-cache\r\n\r\n");
	if (strncmp(service, "POST", strlen("POST")))
		git_buf_puts(buf, "Content-Encoding: chunked");

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
	error = gen_request(&request, url, t->host, "GET", service);
	if (error < GIT_SUCCESS) {
		error = git__throw(error, "Failed to generate request");
		goto cleanup;
	}

	error = gitno_send(t->socket, git_buf_cstr(&request), strlen(git_buf_cstr(&request)), 0);
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
	GIT_UNUSED_ARG(list);
	int error;
	unsigned int i;
	char buff[128];
	gitno_buffer buf;
	git_revwalk *walk;
	git_oid oid;
	const char *prefix = "http://", *url = t->parent.url;
	git_buf request = GIT_BUF_INIT;
	gitno_buffer_setup(&buf, buff, sizeof(buff), t->socket);

	/* TODO: Store url in the transport */
	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	error = setup_walk(&walk, repo);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to setup walk");

	do {
		error = do_connect(t, t->host, t->port);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to connect to host");

		error = gen_request(&request, url, t->host, "POST", "upload-pack");
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to generate request");

		error =  gitno_send(t->socket, request.ptr, request.size, 0);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to send request");

		error =  git_pkt_send_wants(wants, &t->caps, t->socket, 1);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to send wants");


		i = 0;
		while ((error = git_revwalk_next(&oid, walk)) == GIT_SUCCESS) {
			error = git_pkt_send_have(&oid, t->socket, 1);
			if (error < GIT_SUCCESS)
				return git__rethrow(error, "Failed to send have");
			i++;
		}
		if (error < GIT_SUCCESS || i >= 256)
			break;
	} while(1);

	git_revwalk_free(walk);
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
	git_buf_free(&t->buf);
	free(t->heads);
	free(t->content_type);
	free(t->host);
	free(t->port);
	free(t->service);
	free(t->parent.url);
	free(t);
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
