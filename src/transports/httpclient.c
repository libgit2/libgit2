/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2.h"
#include "http_parser.h"
#include "vector.h"
#include "trace.h"
#include "global.h"
#include "httpclient.h"
#include "http.h"
#include "auth.h"
#include "auth_negotiate.h"
#include "auth_ntlm.h"
#include "git2/sys/cred.h"
#include "net.h"
#include "stream.h"
#include "streams/socket.h"
#include "streams/tls.h"
#include "auth.h"

static git_http_auth_scheme auth_schemes[] = {
	{ GIT_HTTP_AUTH_NEGOTIATE, "Negotiate", GIT_CREDTYPE_DEFAULT, git_http_auth_negotiate },
	{ GIT_HTTP_AUTH_NTLM, "NTLM", GIT_CREDTYPE_USERPASS_PLAINTEXT, git_http_auth_ntlm },
	{ GIT_HTTP_AUTH_BASIC, "Basic", GIT_CREDTYPE_USERPASS_PLAINTEXT, git_http_auth_basic },
};

#define GIT_READ_BUFFER_SIZE 8192

typedef struct {
	git_net_url url;
	git_stream *stream;

	git_vector auth_challenges;
	git_http_auth_context *auth_context;
} git_http_server;

typedef enum {
	NONE = 0,
	SENDING_BODY,
	SENT_REQUEST,
	READING_RESPONSE,
	READING_BODY,
	DONE
} http_client_state;

/* Parser state */
typedef enum {
	PARSE_HEADER_NONE = 0,
	PARSE_HEADER_NAME,
	PARSE_HEADER_VALUE,
	PARSE_HEADER_COMPLETE
} parse_header_state;

typedef enum {
	PARSE_STATUS_OK,
	PARSE_STATUS_NO_OUTPUT,
	PARSE_STATUS_ERROR
} parse_status;

typedef struct {
	git_http_client *client;
	git_http_response *response;

	/* Temporary buffers to avoid extra mallocs */
	git_buf parse_header_name;
	git_buf parse_header_value;

	/* Parser state */
	int error;
	parse_status parse_status;

	/* Headers parsing */
	parse_header_state parse_header_state;

	/* Body parsing */
	char *output_buf;       /* Caller's output buffer */
	size_t output_size;     /* Size of caller's output buffer */
	size_t output_written;  /* Bytes we've written to output buffer */
} http_parser_context;

/* HTTP client connection */
struct git_http_client {
	git_http_client_options opts;

	http_client_state state;

	http_parser parser;

	git_http_server server;
	git_http_server proxy;

	unsigned request_count;
	unsigned connected : 1,
	         keepalive : 1,
	         request_chunked : 1;

	/* Temporary buffers to avoid extra mallocs */
	git_buf request_msg;
	git_buf read_buf;

	/* A subset of information from the request */
	size_t request_body_len,
	       request_body_remain;
};

bool git_http_response_is_redirect(git_http_response *response)
{
	return (response->status == 301 ||
	        response->status == 302 ||
	        response->status == 303 ||
		response->status == 307 ||
		response->status == 308);
}

void git_http_response_dispose(git_http_response *response)
{
	assert(response);

	git__free(response->content_type);
	git__free(response->location);

	memset(response, 0, sizeof(git_http_response));
}

static int on_header_complete(http_parser *parser)
{
	http_parser_context *ctx = (http_parser_context *) parser->data;
	git_http_client *client = ctx->client;
	git_http_response *response = ctx->response;

	git_buf *name = &ctx->parse_header_name;
	git_buf *value = &ctx->parse_header_value;

	if (!strcasecmp("Content-Type", name->ptr)) {
		if (response->content_type) {
			git_error_set(GIT_ERROR_NET,
			              "multiple content-type headers");
			return -1;
		}

		response->content_type =
			git__strndup(value->ptr, value->size);
		GIT_ERROR_CHECK_ALLOC(ctx->response->content_type);
	} else if (!strcasecmp("Content-Length", name->ptr)) {
		int64_t len;

		if (response->content_length) {
			git_error_set(GIT_ERROR_NET,
			              "multiple content-length headers");
			return -1;
		}

		if (git__strntol64(&len, value->ptr, value->size,
		                   NULL, 10) < 0 || len < 0) {
			git_error_set(GIT_ERROR_NET,
			              "invalid content-length");
			return -1;
		}

		response->content_length = (size_t)len;
	} else if (!strcasecmp("Proxy-Authenticate", git_buf_cstr(name))) {
		char *dup = git__strndup(value->ptr, value->size);
		GIT_ERROR_CHECK_ALLOC(dup);

		if (git_vector_insert(&client->proxy.auth_challenges, dup) < 0)
			return -1;
	} else if (!strcasecmp("WWW-Authenticate", name->ptr)) {
		char *dup = git__strndup(value->ptr, value->size);
		GIT_ERROR_CHECK_ALLOC(dup);

		if (git_vector_insert(&client->server.auth_challenges, dup) < 0)
			return -1;
	} else if (!strcasecmp("Location", name->ptr)) {
		if (response->location) {
			git_error_set(GIT_ERROR_NET,
				"multiple location headers");
			return -1;
		}

		response->location = git__strndup(value->ptr, value->size);
		GIT_ERROR_CHECK_ALLOC(response->location);
	}

	return 0;
}

static int on_header_field(http_parser *parser, const char *str, size_t len)
{
	http_parser_context *ctx = (http_parser_context *) parser->data;

	switch (ctx->parse_header_state) {
	/*
	 * We last saw a header value, process the name/value pair and
	 * get ready to handle this new name.
	 */
	case PARSE_HEADER_VALUE:
		if (on_header_complete(parser) < 0)
			return ctx->parse_status = PARSE_STATUS_ERROR;

		git_buf_clear(&ctx->parse_header_name);
		git_buf_clear(&ctx->parse_header_value);
		/* Fall through */

	case PARSE_HEADER_NONE:
	case PARSE_HEADER_NAME:
		ctx->parse_header_state = PARSE_HEADER_NAME;

		if (git_buf_put(&ctx->parse_header_name, str, len) < 0)
			return ctx->parse_status = PARSE_STATUS_ERROR;

		break;

	default:
		git_error_set(GIT_ERROR_NET,
		              "header name seen at unexpected time");
		return ctx->parse_status = PARSE_STATUS_ERROR;
	}

	return 0;
}

static int on_header_value(http_parser *parser, const char *str, size_t len)
{
	http_parser_context *ctx = (http_parser_context *) parser->data;

	switch (ctx->parse_header_state) {
	case PARSE_HEADER_NAME:
	case PARSE_HEADER_VALUE:
		ctx->parse_header_state = PARSE_HEADER_VALUE;

		if (git_buf_put(&ctx->parse_header_value, str, len) < 0)
			return ctx->parse_status = PARSE_STATUS_ERROR;

		break;

	default:
		git_error_set(GIT_ERROR_NET,
		              "header value seen at unexpected time");
		return ctx->parse_status = PARSE_STATUS_ERROR;
	}

	return 0;
}

GIT_INLINE(bool) challenge_matches_scheme(
	const char *challenge,
	git_http_auth_scheme *scheme)
{
	const char *scheme_name = scheme->name;
	size_t scheme_len = strlen(scheme_name);

	if (!strncasecmp(challenge, scheme_name, scheme_len) &&
	    (challenge[scheme_len] == '\0' || challenge[scheme_len] == ' '))
		return true;

	return false;
}

static git_http_auth_scheme *scheme_for_challenge(const char *challenge)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(auth_schemes); i++) {
		if (challenge_matches_scheme(challenge, &auth_schemes[i]))
			return &auth_schemes[i];
	}

	return NULL;
}

GIT_INLINE(void) collect_authinfo(
	unsigned int *schemetypes,
	unsigned int *credtypes,
	git_vector *challenges)
{
	git_http_auth_scheme *scheme;
	const char *challenge;
	size_t i;

	*schemetypes = 0;
	*credtypes = 0;

	git_vector_foreach(challenges, i, challenge) {
		if ((scheme = scheme_for_challenge(challenge)) != NULL) {
			*schemetypes |= scheme->type;
			*credtypes |= scheme->credtypes;
		}
	}
}

static int resend_needed(git_http_client *client, git_http_response *response)
{
	git_http_auth_context *auth_context;

	if (response->status == 401 &&
	    (auth_context = client->server.auth_context) &&
	    auth_context->is_complete &&
	    !auth_context->is_complete(auth_context))
		return 1;

	if (response->status == 407 &&
	    (auth_context = client->proxy.auth_context) &&
	    auth_context->is_complete &&
	    !auth_context->is_complete(auth_context))
		return 1;

	return 0;
}

static int on_headers_complete(http_parser *parser)
{
	http_parser_context *ctx = (http_parser_context *) parser->data;

	/* Finalize the last seen header */
	switch (ctx->parse_header_state) {
	case PARSE_HEADER_VALUE:
		if (on_header_complete(parser) < 0)
			return ctx->parse_status = PARSE_STATUS_ERROR;

		/* Fall through */

	case PARSE_HEADER_NONE:
		ctx->parse_header_state = PARSE_HEADER_COMPLETE;
		break;

	default:
		git_error_set(GIT_ERROR_NET,
		              "header completion at unexpected time");
		return ctx->parse_status = PARSE_STATUS_ERROR;
	}

	ctx->response->status = parser->status_code;
	ctx->client->keepalive = http_should_keep_alive(parser);

	/* Prepare for authentication */
	collect_authinfo(&ctx->response->server_auth_schemetypes,
	                 &ctx->response->server_auth_credtypes,
	                 &ctx->client->server.auth_challenges);
	collect_authinfo(&ctx->response->proxy_auth_schemetypes,
	                 &ctx->response->proxy_auth_credtypes,
	                 &ctx->client->proxy.auth_challenges);

	ctx->response->resend_credentials = resend_needed(ctx->client,
	                                                  ctx->response);

	/* Stop parsing. */
	http_parser_pause(parser, 1);

	ctx->client->state = READING_BODY;
	return 0;
}

static int on_body(http_parser *parser, const char *buf, size_t len)
{
	http_parser_context *ctx = (http_parser_context *) parser->data;
	size_t max_len;

	/* Saw data when we expected not to (eg, in consume_response_body) */
	if (ctx->output_buf == NULL && ctx->output_size == 0) {
		ctx->parse_status = PARSE_STATUS_NO_OUTPUT;
		return 0;
	}

	assert(ctx->output_size >= ctx->output_written);

	max_len = min(ctx->output_size - ctx->output_written, len);
	max_len = min(max_len, INT_MAX);

	memcpy(ctx->output_buf + ctx->output_written, buf, max_len);
	ctx->output_written += max_len;

	return 0;
}

static int on_message_complete(http_parser *parser)
{
	http_parser_context *ctx = (http_parser_context *) parser->data;

	ctx->client->state = DONE;
	return 0;
}

GIT_INLINE(int) stream_write(
	git_http_server *server,
	const char *data,
	size_t len)
{
	git_trace(GIT_TRACE_TRACE,
	          "Sending request:\n%.*s", (int)len, data);

	return git_stream__write_full(server->stream, data, len, 0);
}

const char *name_for_method(git_http_method method)
{
	switch (method) {
	case GIT_HTTP_METHOD_GET:
		return "GET";
	case GIT_HTTP_METHOD_POST:
		return "POST";
	}

	return NULL;
}

/*
 * Find the scheme that is suitable for the given credentials, based on the
 * server's auth challenges.
 */
static bool best_scheme_and_challenge(
	git_http_auth_scheme **scheme_out,
	const char **challenge_out,
	git_vector *challenges,
	git_cred *credentials)
{
	const char *challenge;
	size_t i, j;

	for (i = 0; i < ARRAY_SIZE(auth_schemes); i++) {
		git_vector_foreach(challenges, j, challenge) {
			git_http_auth_scheme *scheme = &auth_schemes[i];

			if (challenge_matches_scheme(challenge, scheme) &&
			    (scheme->credtypes & credentials->credtype)) {
				*scheme_out = scheme;
				*challenge_out = challenge;
				return true;
			}
		}
	}

	return false;
}

/*
 * Find the challenge from the server for our current auth context.
 */
static const char *challenge_for_context(
	git_vector *challenges,
	git_http_auth_context *auth_ctx)
{
	const char *challenge;
	size_t i, j;

	for (i = 0; i < ARRAY_SIZE(auth_schemes); i++) {
		if (auth_schemes[i].type == auth_ctx->type) {
			git_http_auth_scheme *scheme = &auth_schemes[i];

			git_vector_foreach(challenges, j, challenge) {
				if (challenge_matches_scheme(challenge, scheme))
					return challenge;
			}
		}
	}

	return NULL;
}

static const char *init_auth_context(
	git_http_server *server,
	git_vector *challenges,
	git_cred *credentials)
{
	git_http_auth_scheme *scheme;
	const char *challenge;
	int error;

	if (!best_scheme_and_challenge(&scheme, &challenge, challenges, credentials)) {
		git_error_set(GIT_ERROR_NET, "could not find appropriate mechanism for credentials");
		return NULL;
	}

	error = scheme->init_context(&server->auth_context, &server->url);

	if (error == GIT_PASSTHROUGH) {
		git_error_set(GIT_ERROR_NET, "'%s' authentication is not supported", scheme->name);
		return NULL;
	}

	return challenge;
}

static void free_auth_context(git_http_server *server)
{
	if (!server->auth_context)
		return;

	if (server->auth_context->free)
		server->auth_context->free(server->auth_context);

	server->auth_context = NULL;
}

static int apply_credentials(
	git_buf *buf,
	git_http_server *server,
	const char *header_name,
	git_cred *credentials)
{
	git_http_auth_context *auth = server->auth_context;
	git_vector *challenges = &server->auth_challenges;
	const char *challenge;
	git_buf token = GIT_BUF_INIT;
	int error = 0;

	/* We've started a new request without creds; free the context. */
	if (auth && !credentials) {
		free_auth_context(server);
		return 0;
	}

	/* We haven't authenticated, nor were we asked to.  Nothing to do. */
	if (!auth && !git_vector_length(challenges))
		return 0;

	if (!auth) {
		challenge = init_auth_context(server, challenges, credentials);
		auth = server->auth_context;

		if (!challenge || !auth) {
			error = -1;
			goto done;
		}
	} else if (auth->set_challenge) {
		challenge = challenge_for_context(challenges, auth);
	}

	if (auth->set_challenge && challenge &&
	    (error = auth->set_challenge(auth, challenge)) < 0)
		goto done;

	if ((error = auth->next_token(&token, auth, credentials)) < 0)
		goto done;

	if (auth->is_complete && auth->is_complete(auth)) {
		/*
		 * If we're done with an auth mechanism with connection affinity,
		 * we don't need to send any more headers and can dispose the context.
		 */
		if (auth->connection_affinity)
			free_auth_context(server);
	} else if (!token.size) {
		git_error_set(GIT_ERROR_NET, "failed to respond to authentication challange");
		error = -1;
		goto done;
	}

	if (token.size > 0)
		error = git_buf_printf(buf, "%s: %s\r\n", header_name, token.ptr);

done:
	git_buf_dispose(&token);
	return error;
}

GIT_INLINE(int) apply_server_credentials(
	git_buf *buf,
	git_http_client *client,
	git_http_request *request)
{
	return apply_credentials(buf,
	                         &client->server,
	                         "Authorization",
	                         request->credentials);
}

GIT_INLINE(int) apply_proxy_credentials(
	git_buf *buf,
	git_http_client *client,
	git_http_request *request)
{
	return apply_credentials(buf,
	                         &client->proxy,
	                         "Proxy-Authorization",
	                         request->proxy_credentials);
}

static int generate_request(
	git_http_client *client,
	git_http_request *request)
{
	git_buf *buf;
	size_t i;
	int error;

	assert(client && request);

	git_buf_clear(&client->request_msg);
	buf = &client->request_msg;

	/* GET|POST path HTTP/1.1 */
	git_buf_puts(buf, name_for_method(request->method));
	git_buf_putc(buf, ' ');

	if (request->proxy && strcmp(request->url->scheme, "https"))
		git_net_url_fmt(buf, request->url);
	else
		git_net_url_fmt_path(buf, request->url);

	git_buf_puts(buf, " HTTP/1.1\r\n");

	git_buf_puts(buf, "User-Agent: ");
	git_http__user_agent(buf);
	git_buf_puts(buf, "\r\n");
	git_buf_printf(buf, "Host: %s", request->url->host);

	if (!git_net_url_is_default_port(request->url))
		git_buf_printf(buf, ":%s", request->url->port);

	git_buf_puts(buf, "\r\n");

	if (request->accept)
		git_buf_printf(buf, "Accept: %s\r\n", request->accept);
	else
		git_buf_puts(buf, "Accept: */*\r\n");

	if (request->content_type)
		git_buf_printf(buf, "Content-Type: %s\r\n",
			request->content_type);

	if (request->chunked)
		git_buf_puts(buf, "Transfer-Encoding: chunked\r\n");

	if (request->content_length > 0)
		git_buf_printf(buf, "Content-Length: %"PRIuZ "\r\n",
			request->content_length);

	if (request->expect_continue)
		git_buf_printf(buf, "Expect: 100-continue\r\n");

	if ((error = apply_server_credentials(buf, client, request)) < 0 ||
	    (error = apply_proxy_credentials(buf, client, request)) < 0)
		return error;

	if (request->custom_headers) {
		for (i = 0; i < request->custom_headers->count; i++) {
			const char *hdr = request->custom_headers->strings[i];

			if (hdr)
				git_buf_printf(buf, "%s\r\n", hdr);
		}
	}

	git_buf_puts(buf, "\r\n");

	if (git_buf_oom(buf))
		return -1;

	return 0;
}

static int check_certificate(
	git_stream *stream,
	git_net_url *url,
	int is_valid,
	git_transport_certificate_check_cb cert_cb,
	void *cert_cb_payload)
{
	git_cert *cert;
	git_error_state last_error = {0};
	int error;

	if ((error = git_stream_certificate(&cert, stream)) < 0)
		return error;

	git_error_state_capture(&last_error, GIT_ECERTIFICATE);

	error = cert_cb(cert, is_valid, url->host, cert_cb_payload);

	if (error == GIT_PASSTHROUGH && !is_valid)
		return git_error_state_restore(&last_error);
	else if (error == GIT_PASSTHROUGH)
		error = 0;
	else if (error && !git_error_last())
		git_error_set(GIT_ERROR_NET,
		              "user rejected certificate for %s", url->host);

	git_error_state_free(&last_error);
	return error;
}

static int stream_connect(
	git_stream *stream,
	git_net_url *url,
	git_transport_certificate_check_cb cert_cb,
	void *cb_payload)
{
	int error;

	GIT_ERROR_CHECK_VERSION(stream, GIT_STREAM_VERSION, "git_stream");

	error = git_stream_connect(stream);

	if (error && error != GIT_ECERTIFICATE)
		return error;

	if (git_stream_is_encrypted(stream) && cert_cb != NULL)
		error = check_certificate(stream, url, !error,
		                          cert_cb, cb_payload);

	return error;
}

static void reset_auth_connection(git_http_server *server)
{
	/*
	 * If we've authenticated and we're doing "normal"
	 * authentication with a request affinity (Basic, Digest)
	 * then we want to _keep_ our context, since authentication
	 * survives even through non-keep-alive connections.  If
	 * we've authenticated and we're doing connection-based
	 * authentication (NTLM, Negotiate) - indicated by the presence
	 * of an `is_complete` callback - then we need to restart
	 * authentication on a new connection.
	 */

	if (server->auth_context &&
	    server->auth_context->connection_affinity)
		free_auth_context(server);
}

/*
 * Updates the server data structure with the new URL; returns 1 if the server
 * has changed and we need to reconnect, returns 0 otherwise.
 */
GIT_INLINE(int) server_setup_from_url(
	git_http_server *server,
	git_net_url *url)
{
	if (!server->url.scheme || strcmp(server->url.scheme, url->scheme) ||
	    !server->url.host || strcmp(server->url.host, url->host) ||
	    !server->url.port || strcmp(server->url.port, url->port)) {
		git__free(server->url.scheme);
		git__free(server->url.host);
		git__free(server->url.port);

		server->url.scheme = git__strdup(url->scheme);
		GIT_ERROR_CHECK_ALLOC(server->url.scheme);

		server->url.host = git__strdup(url->host);
		GIT_ERROR_CHECK_ALLOC(server->url.host);

		server->url.port = git__strdup(url->port);
		GIT_ERROR_CHECK_ALLOC(server->url.port);

		return 1;
	}

	return 0;
}

static int http_client_setup_hosts(
	git_http_client *client,
	git_http_request *request)
{
	int ret, diff = 0;

	assert(client && request && request->url);

	if ((ret = server_setup_from_url(&client->server, request->url)) < 0)
		return ret;

	diff |= ret;

	if (request->proxy &&
	    (ret = server_setup_from_url(&client->proxy, request->proxy)) < 0)
		return ret;

	diff |= ret;

	if (diff)
		client->connected = 0;

	return 0;
}

static void reset_parser(git_http_client *client)
{
	git_buf_clear(&client->read_buf);
}

static int http_client_connect(git_http_client *client)
{
	git_net_url *url;
	git_stream *proxy_stream = NULL, *stream = NULL;
	git_transport_certificate_check_cb cert_cb;
	void *cb_payload;
	int error;

	if (client->connected && client->keepalive &&
	    (client->state == NONE || client->state == DONE))
		return 0;

	git_trace(GIT_TRACE_DEBUG, "Connecting to %s:%s",
		client->server.url.host, client->server.url.port);

	if (client->server.stream) {
		git_stream_close(client->server.stream);
		git_stream_free(client->server.stream);
		client->server.stream = NULL;
	}

	if (client->proxy.stream) {
		git_stream_close(client->proxy.stream);
		git_stream_free(client->proxy.stream);
		client->proxy.stream = NULL;
	}

	reset_auth_connection(&client->server);
	reset_auth_connection(&client->proxy);

	reset_parser(client);

	client->connected = 0;
	client->keepalive = 0;
	client->request_count = 0;

	if (client->proxy.url.host) {
		url = &client->proxy.url;
		cert_cb = client->opts.proxy_certificate_check_cb;
		cb_payload = client->opts.proxy_certificate_check_payload;
	} else {
		url = &client->server.url;
		cert_cb = client->opts.server_certificate_check_cb;
		cb_payload = client->opts.server_certificate_check_payload;
	}

	if (strcasecmp(url->scheme, "https") == 0) {
		error = git_tls_stream_new(&stream, url->host, url->port);
	} else if (strcasecmp(url->scheme, "http") == 0) {
		error = git_socket_stream_new(&stream, url->host, url->port);
	} else {
		git_error_set(GIT_ERROR_NET, "unknown http scheme '%s'",
		              url->scheme);
		error = -1;
	}

	if (error < 0)
		goto on_error;

	if ((error = stream_connect(stream, url, cert_cb, cb_payload)) < 0)
		goto on_error;

	client->proxy.stream = proxy_stream;
	client->server.stream = stream;
	client->connected = 1;
	return 0;

on_error:
	if (stream) {
		git_stream_close(stream);
		git_stream_free(stream);
	}

	if (proxy_stream) {
		git_stream_close(proxy_stream);
		git_stream_free(proxy_stream);
	}

	return error;
}

GIT_INLINE(int) client_read(git_http_client *client)
{
	char *buf = client->read_buf.ptr + client->read_buf.size;
	size_t max_len;
	ssize_t read_len;

	/*
	 * We use a git_buf for convenience, but statically allocate it and
	 * don't resize.  Limit our consumption to INT_MAX since calling
	 * functions use an int return type to return number of bytes read.
	 */
	max_len = client->read_buf.asize - client->read_buf.size;
	max_len = min(max_len, INT_MAX);

	if (max_len == 0) {
		git_error_set(GIT_ERROR_NET, "no room in output buffer");
		return -1;
	}

	read_len = git_stream_read(client->server.stream, buf, max_len);

	if (read_len >= 0) {
		client->read_buf.size += read_len;

		git_trace(GIT_TRACE_TRACE, "Received:\n%.*s",
		          (int)read_len, buf);
	}

	return (int)read_len;
}

static bool parser_settings_initialized;
static http_parser_settings parser_settings;

GIT_INLINE(http_parser_settings *) http_client_parser_settings(void)
{
	if (!parser_settings_initialized) {
		parser_settings.on_header_field = on_header_field;
		parser_settings.on_header_value = on_header_value;
		parser_settings.on_headers_complete = on_headers_complete;
		parser_settings.on_body = on_body;
		parser_settings.on_message_complete = on_message_complete;

		parser_settings_initialized = true;
	}

	return &parser_settings;
}

GIT_INLINE(int) client_read_and_parse(git_http_client *client)
{
	http_parser *parser = &client->parser;
	http_parser_context *ctx = (http_parser_context *) parser->data;
	unsigned char http_errno;
	int read_len;
	size_t parsed_len;

	/*
	 * If we have data in our read buffer, that means we stopped early
	 * when parsing headers.  Use the data in the read buffer instead of
	 * reading more from the socket.
	 */
	if (!client->read_buf.size && (read_len = client_read(client)) < 0)
		return read_len;

	parsed_len = http_parser_execute(parser,
		http_client_parser_settings(),
		client->read_buf.ptr,
		client->read_buf.size);
	http_errno = client->parser.http_errno;

	if (parsed_len > INT_MAX) {
		git_error_set(GIT_ERROR_NET, "unexpectedly large parse");
		return -1;
	}

	if (parser->upgrade) {
		git_error_set(GIT_ERROR_NET, "server requested upgrade");
		return -1;
	}

	if (ctx->parse_status == PARSE_STATUS_ERROR) {
		client->connected = 0;
		return ctx->error ? ctx->error : -1;
	}

	/*
	 * If we finished reading the headers or body, we paused parsing.
	 * Otherwise the parser will start filling the body, or even parse
	 * a new response if the server pipelined us multiple responses.
	 * (This can happen in response to an expect/continue request,
	 * where the server gives you a 100 and 200 simultaneously.)
	 */
	if (http_errno == HPE_PAUSED) {
		/*
		 * http-parser has a "feature" where it will not deliver the
		 * final byte when paused in a callback.  Consume that byte.
		 * https://github.com/nodejs/http-parser/issues/97
		 */
		assert(client->read_buf.size > parsed_len);

		http_parser_pause(parser, 0);

		parsed_len += http_parser_execute(parser,
			http_client_parser_settings(),
			client->read_buf.ptr + parsed_len,
			1);
	}

	/* Most failures will be reported in http_errno */
	else if (parser->http_errno != HPE_OK) {
		git_error_set(GIT_ERROR_NET, "http parser error: %s",
		              http_errno_description(http_errno));
		return -1;
	}

	/* Otherwise we should have consumed the entire buffer. */
	else if (parsed_len != client->read_buf.size) {
		git_error_set(GIT_ERROR_NET,
		              "http parser did not consume entire buffer: %s",
			      http_errno_description(http_errno));
		return -1;
	}

	/* recv returned 0, the server hung up on us */
	else if (!parsed_len) {
		git_error_set(GIT_ERROR_NET, "unexpected EOF");
		return -1;
	}

	git_buf_consume_bytes(&client->read_buf, parsed_len);

	return (int)parsed_len;
}

/*
 * See if we've consumed the entire response body.  If the client was
 * reading the body but did not consume it entirely, it's possible that
 * they knew that the stream had finished (in a git response, seeing a final
 * flush) and stopped reading.  But if the response was chunked, we may have
 * not consumed the final chunk marker.  Consume it to ensure that we don't
 * have it waiting in our socket.  If there's more than just a chunk marker,
 * close the connection.
 */
static void complete_response_body(git_http_client *client)
{
	http_parser_context parser_context = {0};

	/* If we're not keeping alive, don't bother. */
	if (!client->keepalive) {
		client->connected = 0;
		return;
	}

	parser_context.client = client;
	client->parser.data = &parser_context;

	/* If there was an error, just close the connection. */
	if (client_read_and_parse(client) < 0 ||
	    parser_context.error != HPE_OK ||
	    (parser_context.parse_status != PARSE_STATUS_OK &&
	     parser_context.parse_status != PARSE_STATUS_NO_OUTPUT)) {
		git_error_clear();
		client->connected = 0;
	}
}

int git_http_client_send_request(
	git_http_client *client,
	git_http_request *request)
{
	int error = -1;

	assert(client && request);

	/* If the client did not finish reading, clean up the stream. */
	if (client->state == READING_BODY)
		complete_response_body(client);

	http_parser_init(&client->parser, HTTP_RESPONSE);
	git_buf_clear(&client->read_buf);

	if (git_trace_level() >= GIT_TRACE_DEBUG) {
		git_buf url = GIT_BUF_INIT;
		git_net_url_fmt(&url, request->url);
		git_trace(GIT_TRACE_DEBUG, "Sending %s request to %s",
		          name_for_method(request->method),
		          url.ptr ? url.ptr : "<invalid>");
		git_buf_dispose(&url);
	}

	if ((error = http_client_setup_hosts(client, request)) < 0 ||
	    (error = http_client_connect(client)) < 0 ||
	    (error = generate_request(client, request)) < 0 ||
	    (error = stream_write(&client->server,
	                          client->request_msg.ptr,
	                          client->request_msg.size)) < 0)
		goto done;

	if (request->content_length || request->chunked) {
		client->state = SENDING_BODY;
		client->request_body_len = request->content_length;
		client->request_body_remain = request->content_length;
		client->request_chunked = request->chunked;
	} else {
		client->state = SENT_REQUEST;
	}

done:
	return error;
}

int git_http_client_send_body(
	git_http_client *client,
	const char *buffer,
	size_t buffer_len)
{
	git_http_server *server;
	git_buf hdr = GIT_BUF_INIT;
	int error;

	assert(client && client->state == SENDING_BODY);

	if (!buffer_len)
		return 0;

	server = &client->server;

	if (client->request_body_len) {
		assert(buffer_len <= client->request_body_remain);

		if ((error = stream_write(server, buffer, buffer_len)) < 0)
			goto done;

		client->request_body_remain -= buffer_len;
	} else {
		if ((error = git_buf_printf(&hdr, "%" PRIxZ "\r\n", buffer_len)) < 0 ||
		    (error = stream_write(server, hdr.ptr, hdr.size)) < 0 ||
		    (error = stream_write(server, buffer, buffer_len)) < 0 ||
		    (error = stream_write(server, "\r\n", 2)) < 0)
			goto done;
	}

done:
	git_buf_dispose(&hdr);
	return error;
}

static int complete_request(git_http_client *client)
{
	int error = 0;

	assert(client && client->state == SENDING_BODY);

	if (client->request_body_len && client->request_body_remain) {
		git_error_set(GIT_ERROR_NET, "truncated write");
		error = -1;
	} else if (client->request_chunked) {
		error = stream_write(&client->server, "0\r\n\r\n", 5);
	}

	return error;
}

int git_http_client_read_response(
	git_http_response *response,
	git_http_client *client)
{
	http_parser_context parser_context = {0};
	int error;

	assert(response && client);

	if (client->state == SENDING_BODY) {
		if ((error = complete_request(client)) < 0)
			goto done;
	} else if (client->state != SENT_REQUEST) {
		git_error_set(GIT_ERROR_NET, "client is in invalid state");
		error = -1;
		goto done;
	}

	git_http_response_dispose(response);

	git_vector_free_deep(&client->server.auth_challenges);
	git_vector_free_deep(&client->proxy.auth_challenges);

	client->state = READING_RESPONSE;
	client->parser.data = &parser_context;

	parser_context.client = client;
	parser_context.response = response;

	while (client->state == READING_RESPONSE) {
		if ((error = client_read_and_parse(client)) < 0)
			goto done;
	}

	assert(client->state == READING_BODY || client->state == DONE);

done:
	git_buf_dispose(&parser_context.parse_header_name);
	git_buf_dispose(&parser_context.parse_header_value);

	return error;
}

int git_http_client_read_body(
	git_http_client *client,
	char *buffer,
	size_t buffer_size)
{
	http_parser_context parser_context = {0};
	int error = 0;

	if (client->state == DONE)
		return 0;

	if (client->state != READING_BODY) {
		git_error_set(GIT_ERROR_NET, "client is in invalid state");
		return -1;
	}

	/*
	 * Now we'll read from the socket and http_parser will pipeline the
	 * data directly to the client.
	 */

	parser_context.client = client;
	parser_context.output_buf = buffer;
	parser_context.output_size = buffer_size;

	client->parser.data = &parser_context;

	/*
	 * Clients expect to get a non-zero amount of data from us.
	 * With a sufficiently small buffer, one might only read a chunk
	 * length.  Loop until we actually have data to return.
	 */
	while (!parser_context.output_written) {
		error = client_read_and_parse(client);

		if (error <= 0)
			goto done;
	}

	assert(parser_context.output_written <= INT_MAX);
	error = (int)parser_context.output_written;

done:
	if (error < 0)
		client->connected = 0;

	return error;
}

int git_http_client_skip_body(git_http_client *client)
{
	http_parser_context parser_context = {0};
	int error;

	if (client->state == DONE)
		return 0;

	if (client->state != READING_BODY) {
		git_error_set(GIT_ERROR_NET, "client is in invalid state");
		return -1;
	}

	parser_context.client = client;
	client->parser.data = &parser_context;

	do {
		error = client_read_and_parse(client);

		if (parser_context.error != HPE_OK ||
		    (parser_context.parse_status != PARSE_STATUS_OK &&
		     parser_context.parse_status != PARSE_STATUS_NO_OUTPUT)) {
			git_error_set(GIT_ERROR_NET,
			              "unexpected data handled in callback");
			error = -1;
		}
	} while (!error);

	if (error < 0)
		client->connected = 0;

	return error;
}

/*
 * Create an http_client capable of communicating with the given remote
 * host.
 */
int git_http_client_new(
	git_http_client **out,
	git_http_client_options *opts)
{
	git_http_client *client;

	assert(out);

	client = git__calloc(1, sizeof(git_http_client));
	GIT_ERROR_CHECK_ALLOC(client);

	git_buf_init(&client->read_buf, GIT_READ_BUFFER_SIZE);
	GIT_ERROR_CHECK_ALLOC(client->read_buf.ptr);

	if (opts)
		memcpy(&client->opts, opts, sizeof(git_http_client_options));

	*out = client;
	return 0;
}

GIT_INLINE(void) http_server_close(git_http_server *server)
{
	if (server->stream) {
		git_stream_close(server->stream);
		git_stream_free(server->stream);
		server->stream = NULL;
	}

	git_net_url_dispose(&server->url);

	git_vector_free_deep(&server->auth_challenges);
	free_auth_context(server);
}

static void http_client_close(git_http_client *client)
{
	http_server_close(&client->server);
	http_server_close(&client->proxy);

	git_buf_dispose(&client->request_msg);

	client->state = 0;
	client->request_count = 0;
	client->connected = 0;
	client->keepalive = 0;
}

void git_http_client_free(git_http_client *client)
{
	if (!client)
		return;

	http_client_close(client);
	git_buf_dispose(&client->read_buf);
	git__free(client);
}
