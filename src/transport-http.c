/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdlib.h>
#include "git2.h"
#include "http_parser.h"

#include "transport.h"
#include "common.h"
#include "netops.h"
#include "buffer.h"
#include "pkt.h"

typedef struct {
	git_transport parent;
	git_vector refs;
	int socket;
	git_buf buf;
	git_remote_head **heads;
	int error;
	http_parser parser;
	int transfer_finished :1;
	char *content_type;
	char *host;
	char *port;
	char *service;
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

	if (git_buf_oom(buf))
		return GIT_ENOMEM;

	return GIT_SUCCESS;
}

static int do_connect(transport_http *t, const char *host, const char *port)
{
	int s = -1;

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
static enum {
	FIELD,
	VALUE,
	NONE
} last_cb = NONE;

static int ct_found, ct_finished;
static const char *typestr = "Content-Type";

static int on_header_field(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;

	if (last_cb == VALUE && ct_found) {
		ct_finished = 1;
		ct_found = 0;
		t->content_type = git__strdup(git_buf_cstr(buf));
		if (t->content_type == NULL)
			return t->error = GIT_ENOMEM;
		git_buf_clear(buf);
	}

	if (ct_found) {
		last_cb = FIELD;
		return 0;
	}

	if (last_cb != FIELD)
		git_buf_clear(buf);

	git_buf_put(buf, str, len);
	last_cb = FIELD;

	return git_buf_oom(buf);
}

static int on_header_value(http_parser *parser, const char *str, size_t len)
{
	transport_http *t = (transport_http *) parser->data;
	git_buf *buf = &t->buf;

	if (ct_finished) {
		last_cb = VALUE;
		return 0;
	}

	if (last_cb == VALUE)
		git_buf_put(buf, str, len);

	if (last_cb == FIELD && !strcmp(git_buf_cstr(buf), typestr)) {
		ct_found = 1;
		git_buf_clear(buf);
		git_buf_put(buf, str, len);
	}

	last_cb = VALUE;

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

static int http_close(git_transport *transport)
{
	transport_http *t = (transport_http *) transport;
	int error;

	error = close(t->socket);
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
	t->parent.close = http_close;
	t->parent.free = http_free;

	*out = (git_transport *) t;

	return GIT_SUCCESS;
}
