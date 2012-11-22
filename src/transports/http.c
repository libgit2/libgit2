/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef GIT_WINHTTP

#include "git2.h"
#include "http_parser.h"
#include "buffer.h"
#include "netops.h"
#include "smart.h"

static const char *prefix_http = "http://";
static const char *prefix_https = "https://";
static const char *upload_pack_service = "upload-pack";
static const char *upload_pack_ls_service_url = "/info/refs?service=git-upload-pack";
static const char *upload_pack_service_url = "/git-upload-pack";
static const char *get_verb = "GET";
static const char *post_verb = "POST";
static const char *basic_authtype = "Basic";

#define OWNING_SUBTRANSPORT(s) ((http_subtransport *)(s)->parent.subtransport)

#define PARSE_ERROR_GENERIC	-1
#define PARSE_ERROR_REPLAY	-2

enum last_cb {
	NONE,
	FIELD,
	VALUE
};

typedef enum {
	GIT_HTTP_AUTH_BASIC = 1,
} http_authmechanism_t;

typedef struct {
	git_smart_subtransport_stream parent;
	const char *service;
	const char *service_url;
	const char *verb;
	unsigned sent_request : 1;
} http_stream;

typedef struct {
	git_smart_subtransport parent;
	transport_smart *owner;
	gitno_socket socket;
	const char *path;
	char *host;
	char *port;
	git_cred *cred;
	http_authmechanism_t auth_mechanism;
	unsigned connected : 1,
		use_ssl : 1;

	/* Parser structures */
	http_parser parser;
	http_parser_settings settings;
	gitno_buffer parse_buffer;
	git_buf parse_header_name;
	git_buf parse_header_value;
	char parse_buffer_data[2048];
	char *content_type;
	git_vector www_authenticate;
	enum last_cb last_cb;
	int parse_error;
	unsigned parse_finished : 1;
} http_subtransport;

typedef struct {
	http_stream *s;
	http_subtransport *t;

	/* Target buffer details from read() */
	char *buffer;
	size_t buf_size;
	size_t *bytes_read;
} parser_context;

static int apply_basic_credential(git_buf *buf, git_cred *cred)
{
	git_cred_userpass_plaintext *c = (git_cred_userpass_plaintext *)cred;
	git_buf raw = GIT_BUF_INIT;
	int error = -1;

	git_buf_printf(&raw, "%s:%s", c->username, c->password);

	if (git_buf_oom(&raw) ||
		git_buf_puts(buf, "Authorization: Basic ") < 0 ||
		git_buf_put_base64(buf, git_buf_cstr(&raw), raw.size) < 0 ||
		git_buf_puts(buf, "\r\n") < 0)
		goto on_error;

	error = 0;

on_error:
	if (raw.size)
		memset(raw.ptr, 0x0, raw.size);

	git_buf_free(&raw);
	return error;
}

static int gen_request(
	git_buf *buf,
	const char *path,
	const char *host,
	git_cred *cred,
	http_authmechanism_t auth_mechanism,
	const char *op,
	const char *service,
	const char *service_url,
	ssize_t content_length)
{
	if (!path)
		path = "/";

	git_buf_printf(buf, "%s %s%s HTTP/1.1\r\n", op, path, service_url);
	git_buf_puts(buf, "User-Agent: git/1.0 (libgit2 " LIBGIT2_VERSION ")\r\n");
	git_buf_printf(buf, "Host: %s\r\n", host);
	if (content_length > 0) {
		git_buf_printf(buf, "Accept: application/x-git-%s-result\r\n", service);
		git_buf_printf(buf, "Content-Type: application/x-git-%s-request\r\n", service);
		git_buf_printf(buf, "Content-Length: %"PRIuZ "\r\n", content_length);
	} else {
		git_buf_puts(buf, "Accept: */*\r\n");
	}

	/* Apply credentials to the request */
	if (cred && cred->credtype == GIT_CREDTYPE_USERPASS_PLAINTEXT &&
		auth_mechanism == GIT_HTTP_AUTH_BASIC &&
		apply_basic_credential(buf, cred) < 0)
		return -1;

	git_buf_puts(buf, "\r\n");

	if (git_buf_oom(buf))
		return -1;

	return 0;
}

static int parse_unauthorized_response(
	git_vector *www_authenticate,
	int *allowed_types,
	http_authmechanism_t *auth_mechanism)
{
	unsigned i;
	char *entry;

	git_vector_foreach(www_authenticate, i, entry) {
		if (!strncmp(entry, basic_authtype, 5) &&
			(entry[5] == '\0' || entry[5] == ' ')) {
			*allowed_types |= GIT_CREDTYPE_USERPASS_PLAINTEXT;
			*auth_mechanism = GIT_HTTP_AUTH_BASIC;
		}
	}

	return 0;
}

static int on_header_ready(http_subtransport *t)
{
	git_buf *name = &t->parse_header_name;
	git_buf *value = &t->parse_header_value;
	char *dup;

	if (!t->content_type && !strcmp("Content-Type", git_buf_cstr(name))) {
		t->content_type = git__strdup(git_buf_cstr(value));
		GITERR_CHECK_ALLOC(t->content_type);
	}
	else if (!strcmp("WWW-Authenticate", git_buf_cstr(name))) {
		dup = git__strdup(git_buf_cstr(value));
		GITERR_CHECK_ALLOC(dup);
		git_vector_insert(&t->www_authenticate, dup);
	}

	return 0;
}

static int on_header_field(http_parser *parser, const char *str, size_t len)
{
	parser_context *ctx = (parser_context *) parser->data;
	http_subtransport *t = ctx->t;

	/* Both parse_header_name and parse_header_value are populated
	 * and ready for consumption */
	if (VALUE == t->last_cb)
		if (on_header_ready(t) < 0)
			return t->parse_error = PARSE_ERROR_GENERIC;

	if (NONE == t->last_cb || VALUE == t->last_cb)
		git_buf_clear(&t->parse_header_name);

	if (git_buf_put(&t->parse_header_name, str, len) < 0)
		return t->parse_error = PARSE_ERROR_GENERIC;

	t->last_cb = FIELD;
	return 0;
}

static int on_header_value(http_parser *parser, const char *str, size_t len)
{
	parser_context *ctx = (parser_context *) parser->data;
	http_subtransport *t = ctx->t;

	assert(NONE != t->last_cb);

	if (FIELD == t->last_cb)
		git_buf_clear(&t->parse_header_value);

	if (git_buf_put(&t->parse_header_value, str, len) < 0)
		return t->parse_error = PARSE_ERROR_GENERIC;

	t->last_cb = VALUE;
	return 0;
}

static int on_headers_complete(http_parser *parser)
{
	parser_context *ctx = (parser_context *) parser->data;
	http_subtransport *t = ctx->t;
	http_stream *s = ctx->s;
	git_buf buf = GIT_BUF_INIT;

	/* Both parse_header_name and parse_header_value are populated
	 * and ready for consumption. */
	if (VALUE == t->last_cb)
		if (on_header_ready(t) < 0)
			return t->parse_error = PARSE_ERROR_GENERIC;

	/* Check for an authentication failure. */
	if (parser->status_code == 401 &&
		get_verb == s->verb &&
		t->owner->cred_acquire_cb) {
		int allowed_types = 0;

		if (parse_unauthorized_response(&t->www_authenticate,
			&allowed_types, &t->auth_mechanism) < 0)
			return t->parse_error = PARSE_ERROR_GENERIC;

		if (allowed_types &&
			(!t->cred || 0 == (t->cred->credtype & allowed_types))) {

			if (t->owner->cred_acquire_cb(&t->cred,
					t->owner->url,
					allowed_types) < 0)
				return PARSE_ERROR_GENERIC;

			assert(t->cred);

			/* Successfully acquired a credential. */
			return t->parse_error = PARSE_ERROR_REPLAY;
		}
	}

	/* Check for a 200 HTTP status code. */
	if (parser->status_code != 200) {
		giterr_set(GITERR_NET,
			"Unexpected HTTP status code: %d",
			parser->status_code);
		return t->parse_error = PARSE_ERROR_GENERIC;
	}

	/* The response must contain a Content-Type header. */
	if (!t->content_type) {
		giterr_set(GITERR_NET, "No Content-Type header in response");
		return t->parse_error = PARSE_ERROR_GENERIC;
	}

	/* The Content-Type header must match our expectation. */
	if (get_verb == s->verb)
		git_buf_printf(&buf,
			"application/x-git-%s-advertisement",
			ctx->s->service);
	else
		git_buf_printf(&buf,
			"application/x-git-%s-result",
			ctx->s->service);

	if (git_buf_oom(&buf))
		return t->parse_error = PARSE_ERROR_GENERIC;

	if (strcmp(t->content_type, git_buf_cstr(&buf))) {
		git_buf_free(&buf);
		giterr_set(GITERR_NET,
			"Invalid Content-Type: %s",
			t->content_type);
		return t->parse_error = PARSE_ERROR_GENERIC;
	}

	git_buf_free(&buf);

	return 0;
}

static int on_message_complete(http_parser *parser)
{
	parser_context *ctx = (parser_context *) parser->data;
	http_subtransport *t = ctx->t;

	t->parse_finished = 1;

	return 0;
}

static int on_body_fill_buffer(http_parser *parser, const char *str, size_t len)
{
	parser_context *ctx = (parser_context *) parser->data;
	http_subtransport *t = ctx->t;

	if (ctx->buf_size < len) {
		giterr_set(GITERR_NET, "Can't fit data in the buffer");
		return t->parse_error = PARSE_ERROR_GENERIC;
	}

	memcpy(ctx->buffer, str, len);
	*(ctx->bytes_read) += len;
	ctx->buffer += len;
	ctx->buf_size -= len;

	return 0;
}

static void clear_parser_state(http_subtransport *t)
{
	unsigned i;
	char *entry;

	http_parser_init(&t->parser, HTTP_RESPONSE);
	gitno_buffer_setup(&t->socket,
		&t->parse_buffer,
		t->parse_buffer_data,
		sizeof(t->parse_buffer_data));

	t->last_cb = NONE;
	t->parse_error = 0;
	t->parse_finished = 0;

	git_buf_free(&t->parse_header_name);
	git_buf_init(&t->parse_header_name, 0);

	git_buf_free(&t->parse_header_value);
	git_buf_init(&t->parse_header_value, 0);

	git__free(t->content_type);
	t->content_type = NULL;

	git_vector_foreach(&t->www_authenticate, i, entry)
		git__free(entry);

	git_vector_free(&t->www_authenticate);
}

static int http_stream_read(
	git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	http_stream *s = (http_stream *)stream;
	http_subtransport *t = OWNING_SUBTRANSPORT(s);
	git_buf request = GIT_BUF_INIT;
	parser_context ctx;

replay:
	*bytes_read = 0;

	assert(t->connected);

	if (!s->sent_request) {
		clear_parser_state(t);

		if (gen_request(&request, t->path, t->host,
				t->cred, t->auth_mechanism, s->verb,
				s->service, s->service_url, 0) < 0) {
			giterr_set(GITERR_NET, "Failed to generate request");
			return -1;
		}

		if (gitno_send(&t->socket, request.ptr, request.size, 0) < 0) {
			git_buf_free(&request);
			return -1;
		}

		git_buf_free(&request);
		s->sent_request = 1;
	}

	t->parse_buffer.offset = 0;

	if (t->parse_finished)
		return 0;

	if (gitno_recv(&t->parse_buffer) < 0)
		return -1;

	/* This call to http_parser_execute will result in invocations of the on_*
	 * family of callbacks. The most interesting of these is
	 * on_body_fill_buffer, which is called when data is ready to be copied
	 * into the target buffer. We need to marshal the buffer, buf_size, and
	 * bytes_read parameters to this callback. */
	ctx.t = t;
	ctx.s = s;
	ctx.buffer = buffer;
	ctx.buf_size = buf_size;
	ctx.bytes_read = bytes_read;

	/* Set the context, call the parser, then unset the context. */
	t->parser.data = &ctx;

	http_parser_execute(&t->parser,
		&t->settings,
		t->parse_buffer.data,
		t->parse_buffer.offset);

	t->parser.data = NULL;

	/* If there was a handled authentication failure, then parse_error
	 * will have signaled us that we should replay the request. */
	if (PARSE_ERROR_REPLAY == t->parse_error) {
		s->sent_request = 0;
		goto replay;
	}

	if (t->parse_error < 0)
		return -1;

	return 0;
}

static int http_stream_write(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
	http_stream *s = (http_stream *)stream;
	http_subtransport *t = OWNING_SUBTRANSPORT(s);
	git_buf request = GIT_BUF_INIT;

	assert(t->connected);

	/* Since we have to write the Content-Length header up front, we're
	 * basically limited to a single call to write() per request. */
	assert(!s->sent_request);

	if (!s->sent_request) {
		clear_parser_state(t);

		if (gen_request(&request, t->path, t->host,
				t->cred, t->auth_mechanism, s->verb,
				s->service, s->service_url, len) < 0) {
			giterr_set(GITERR_NET, "Failed to generate request");
			return -1;
		}

		if (gitno_send(&t->socket, request.ptr, request.size, 0) < 0)
			goto on_error;

		if (len && gitno_send(&t->socket, buffer, len, 0) < 0)
			goto on_error;

		git_buf_free(&request);
		s->sent_request = 1;
	}

	return 0;

on_error:
	git_buf_free(&request);
	return -1;
}

static void http_stream_free(git_smart_subtransport_stream *stream)
{
	http_stream *s = (http_stream *)stream;

	git__free(s);
}

static int http_stream_alloc(http_subtransport *t,
	git_smart_subtransport_stream **stream)
{
	http_stream *s;

	if (!stream)
		return -1;

	s = (http_stream *)git__calloc(sizeof(http_stream), 1);
	GITERR_CHECK_ALLOC(s);

	s->parent.subtransport = &t->parent;
	s->parent.read = http_stream_read;
	s->parent.write = http_stream_write;
	s->parent.free = http_stream_free;

	*stream = (git_smart_subtransport_stream *)s;
	return 0;
}

static int http_uploadpack_ls(
	http_subtransport *t,
	git_smart_subtransport_stream **stream)
{
	http_stream *s;

	if (http_stream_alloc(t, stream) < 0)
		return -1;

	s = (http_stream *)*stream;

	s->service = upload_pack_service;
	s->service_url = upload_pack_ls_service_url;
	s->verb = get_verb;

	return 0;
}

static int http_uploadpack(
	http_subtransport *t,
	git_smart_subtransport_stream **stream)
{
	http_stream *s;

	if (http_stream_alloc(t, stream) < 0)
		return -1;

	s = (http_stream *)*stream;

	s->service = upload_pack_service;
	s->service_url = upload_pack_service_url;
	s->verb = post_verb;

	return 0;
}

static int http_action(
	git_smart_subtransport_stream **stream,
	git_smart_subtransport *smart_transport,
	const char *url,
	git_smart_service_t action)
{
	http_subtransport *t = (http_subtransport *)smart_transport;
	const char *default_port;
	int flags = 0, ret;

	if (!stream)
		return -1;

	if (!t->host || !t->port || !t->path) {
		if (!git__prefixcmp(url, prefix_http)) {
			url = url + strlen(prefix_http);
			default_port = "80";
		}

		if (!git__prefixcmp(url, prefix_https)) {
			url += strlen(prefix_https);
			default_port = "443";
			t->use_ssl = 1;
		}

		if ((ret = gitno_extract_host_and_port(&t->host, &t->port,
				url, default_port)) < 0)
			return ret;

		t->path = strchr(url, '/');
	}

	if (!t->connected || !http_should_keep_alive(&t->parser)) {
		if (t->use_ssl) {
			int transport_flags;

			if (t->owner->parent.read_flags(&t->owner->parent, &transport_flags) < 0)
				return -1;

			flags |= GITNO_CONNECT_SSL;

			if (GIT_TRANSPORTFLAGS_NO_CHECK_CERT & transport_flags)
				flags |= GITNO_CONNECT_SSL_NO_CHECK_CERT;
		}

		if (gitno_connect(&t->socket, t->host, t->port, flags) < 0)
			return -1;

		t->connected = 1;
	}

	t->parse_finished = 0;
	t->parse_error = 0;

	switch (action)
	{
		case GIT_SERVICE_UPLOADPACK_LS:
			return http_uploadpack_ls(t, stream);

		case GIT_SERVICE_UPLOADPACK:
			return http_uploadpack(t, stream);
	}

	*stream = NULL;
	return -1;
}

static void http_free(git_smart_subtransport *smart_transport)
{
	http_subtransport *t = (http_subtransport *) smart_transport;

	clear_parser_state(t);

	if (t->socket.socket)
		gitno_close(&t->socket);

	if (t->cred) {
		t->cred->free(t->cred);
		t->cred = NULL;
	}

	git__free(t->host);
	git__free(t->port);
	git__free(t);
}

int git_smart_subtransport_http(git_smart_subtransport **out, git_transport *owner)
{
	http_subtransport *t;

	if (!out)
		return -1;

	t = (http_subtransport *)git__calloc(sizeof(http_subtransport), 1);
	GITERR_CHECK_ALLOC(t);

	t->owner = (transport_smart *)owner;
	t->parent.action = http_action;
	t->parent.free = http_free;

	t->settings.on_header_field = on_header_field;
	t->settings.on_header_value = on_header_value;
	t->settings.on_headers_complete = on_headers_complete;
	t->settings.on_body = on_body_fill_buffer;
	t->settings.on_message_complete = on_message_complete;

	*out = (git_smart_subtransport *) t;
	return 0;
}

#endif /* !GIT_WINHTTP */
