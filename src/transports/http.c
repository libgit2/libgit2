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
#if GIT_WINHTTP
# include <winhttp.h>
# pragma comment(lib, "winhttp.lib")
#endif

#define WIDEN2(s) L ## s
#define WIDEN(s) WIDEN2(s)

enum last_cb {
	NONE,
	FIELD,
	VALUE
};

typedef struct {
	git_transport parent;
	http_parser_settings settings;
	git_buf buf;
	int error;
	int transfer_finished :1,
		ct_found :1,
		ct_finished :1,
		pack_ready :1;
	enum last_cb last_cb;
	http_parser parser;
	char *content_type;
	char *path;
	char *host;
	char *port;
	char *service;
	char buffer[65536];
#ifdef GIT_WIN32
	WSADATA wsd;
#endif
#ifdef GIT_WINHTTP
	HINTERNET session;
	HINTERNET connection;
	HINTERNET request;
#endif
} transport_http;

static int gen_request(git_buf *buf, const char *path, const char *host, const char *op,
                       const char *service, ssize_t content_length, int ls)
{
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

static int send_request(transport_http *t, const char *service, void *data, ssize_t content_length, int ls)
{
#ifndef GIT_WINHTTP
	git_buf request = GIT_BUF_INIT;
	const char *verb;
	int error = -1;

	verb = ls ? "GET" : "POST";
	/* Generate and send the HTTP request */
	if (gen_request(&request, t->path, t->host, verb, service, content_length, ls) < 0) {
		giterr_set(GITERR_NET, "Failed to generate request");
		return -1;
	}


	if (gitno_send((git_transport *) t, request.ptr, request.size, 0) < 0)
		goto cleanup;

	if (content_length) {
		if (gitno_send((git_transport *) t, data, content_length, 0) < 0)
			goto cleanup;
	}

	error = 0;

cleanup:
	git_buf_free(&request);
	return error;

#else
	wchar_t *verb;
	wchar_t url[GIT_WIN_PATH], ct[GIT_WIN_PATH];
	git_buf buf = GIT_BUF_INIT;
	BOOL ret;
	DWORD flags;
	void *buffer;
	wchar_t *types[] = {
		L"*/*",
		NULL,
	};

	verb = ls ? L"GET" : L"POST";
	buffer = data ? data : WINHTTP_NO_REQUEST_DATA;
	flags = t->parent.use_ssl ? WINHTTP_FLAG_SECURE : 0;

	if (ls)
		git_buf_printf(&buf, "%s/info/refs?service=git-%s", t->path, service);
	else
		git_buf_printf(&buf, "%s/git-%s", t->path, service);

	if (git_buf_oom(&buf))
		return -1;

	git__utf8_to_16(url, GIT_WIN_PATH, git_buf_cstr(&buf));

	t->request = WinHttpOpenRequest(t->connection, verb, url, NULL, WINHTTP_NO_REFERER, types, flags);
	if (t->request == NULL) {
		git_buf_free(&buf);
		giterr_set(GITERR_OS, "Failed to open request");
		return -1;
	}

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "Content-Type: application/x-git-%s-request", service) < 0)
		goto on_error;

	git__utf8_to_16(ct, GIT_WIN_PATH, git_buf_cstr(&buf));

	if (WinHttpAddRequestHeaders(t->request, ct, (ULONG) -1L, WINHTTP_ADDREQ_FLAG_ADD) == FALSE) {
		giterr_set(GITERR_OS, "Failed to add a header to the request");
		goto on_error;
	}

	if (!t->parent.check_cert) {
		int flags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA;
		if (WinHttpSetOption(t->request, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags)) == FALSE) {
			giterr_set(GITERR_OS, "Failed to set options to ignore cert errors");
			goto on_error;
		}
	}

	if (WinHttpSendRequest(t->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		data, (DWORD)content_length, (DWORD)content_length, 0) == FALSE) {
		giterr_set(GITERR_OS, "Failed to send request");
		goto on_error;
	}

	ret = WinHttpReceiveResponse(t->request, NULL);
	if (ret == FALSE) {
		giterr_set(GITERR_OS, "Failed to receive response");
		goto on_error;
	}

	return 0;

on_error:
	git_buf_free(&buf);
	if (t->request)
		WinHttpCloseHandle(t->request);
	t->request = NULL;
	return -1;
#endif
}

static int do_connect(transport_http *t)
{
#ifndef GIT_WINHTTP
	if (t->parent.connected && http_should_keep_alive(&t->parser))
		return 0;

	if (gitno_connect((git_transport *) t, t->host, t->port) < 0)
		return -1;

	t->parent.connected = 1;

	return 0;
#else
	wchar_t *ua = L"git/1.0 (libgit2 " WIDEN(LIBGIT2_VERSION) L")";
	wchar_t host[GIT_WIN_PATH];
	int32_t port;

	t->session = WinHttpOpen(ua, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (t->session == NULL) {
		giterr_set(GITERR_OS, "Failed to init WinHTTP");
		goto on_error;
	}

	git__utf8_to_16(host, GIT_WIN_PATH, t->host);

	if (git__strtol32(&port, t->port, NULL, 10) < 0)
		goto on_error;

	t->connection = WinHttpConnect(t->session, host, port, 0);
	if (t->connection == NULL) {
		giterr_set(GITERR_OS, "Failed to connect to host");
		goto on_error;
	}

	t->parent.connected = 1;
	return 0;

on_error:
	if (t->session) {
		WinHttpCloseHandle(t->session);
		t->session = NULL;
	}
	return -1;
#endif
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

static int on_body_fill_buffer(http_parser *parser, const char *str, size_t len)
{
	git_transport *transport = (git_transport *) parser->data;
	transport_http *t = (transport_http *) parser->data;
	gitno_buffer *buf = &transport->buffer;

	if (buf->len - buf->offset < len) {
		giterr_set(GITERR_NET, "Can't fit data in the buffer");
		return t->error = -1;
	}

	memcpy(buf->data + buf->offset, str, len);
	buf->offset += len;

	return 0;
}

static int http_recv_cb(gitno_buffer *buf)
{
	git_transport *transport = (git_transport *) buf->cb_data;
	transport_http *t = (transport_http *) transport;
	size_t old_len;
	char buffer[2048];
#ifdef GIT_WINHTTP
	DWORD recvd;
#else
	gitno_buffer inner;
	int error;
#endif

	if (t->transfer_finished)
		return 0;

#ifndef GIT_WINHTTP
	gitno_buffer_setup(transport, &inner, buffer, sizeof(buffer));

	if ((error = gitno_recv(&inner)) < 0)
		return -1;

	old_len = buf->offset;
	http_parser_execute(&t->parser, &t->settings, inner.data, inner.offset);
	if (t->error < 0)
		return t->error;
#else
	old_len = buf->offset;
	if (WinHttpReadData(t->request, buffer, sizeof(buffer), &recvd) == FALSE) {
		giterr_set(GITERR_OS, "Failed to read data from the network");
		return t->error = -1;
	}

	if (buf->len - buf->offset < recvd) {
		giterr_set(GITERR_NET, "Can't fit data in the buffer");
		return t->error = -1;
	}

	memcpy(buf->data + buf->offset, buffer, recvd);
	buf->offset += recvd;
#endif

	return (int)(buf->offset - old_len);
}

/* Set up the gitno_buffer so calling gitno_recv() grabs data from the HTTP response */
static void setup_gitno_buffer(git_transport *transport)
{
	transport_http *t = (transport_http *) transport;

	/* WinHTTP takes care of this for us */
#ifndef GIT_WINHTTP
	http_parser_init(&t->parser, HTTP_RESPONSE);
	t->parser.data = t;
	t->transfer_finished = 0;
	memset(&t->settings, 0x0, sizeof(http_parser_settings));
	t->settings.on_header_field = on_header_field;
	t->settings.on_header_value = on_header_value;
	t->settings.on_headers_complete = on_headers_complete;
	t->settings.on_body = on_body_fill_buffer;
	t->settings.on_message_complete = on_message_complete;
#endif

	gitno_buffer_setup_callback(transport, &transport->buffer, t->buffer, sizeof(t->buffer), http_recv_cb, t);
}

static int http_connect(git_transport *transport, int direction)
{
	transport_http *t = (transport_http *) transport;
	int ret;
	git_buf request = GIT_BUF_INIT;
	const char *service = "upload-pack";
	const char *url = t->parent.url, *prefix_http = "http://", *prefix_https = "https://";
	const char *default_port;
	git_pkt *pkt;

	if (direction == GIT_DIR_PUSH) {
		giterr_set(GITERR_NET, "Pushing over HTTP is not implemented");
		return -1;
	}

	t->parent.direction = direction;

	if (!git__prefixcmp(url, prefix_http)) {
		url = t->parent.url + strlen(prefix_http);
		default_port = "80";
	}

	if (!git__prefixcmp(url, prefix_https)) {
		url += strlen(prefix_https);
		default_port = "443";
	}

	t->path = strchr(url, '/');

	if ((ret = gitno_extract_host_and_port(&t->host, &t->port, url, default_port)) < 0)
		goto cleanup;

	t->service = git__strdup(service);
	GITERR_CHECK_ALLOC(t->service);

	if ((ret = do_connect(t)) < 0)
		goto cleanup;

	if ((ret = send_request(t, "upload-pack", NULL, 0, 1)) < 0)
		goto cleanup;

	setup_gitno_buffer(transport);
	if ((ret = git_protocol_store_refs(transport, 2)) < 0)
		goto cleanup;

	pkt = git_vector_get(&transport->refs, 0);
	if (pkt == NULL || pkt->type != GIT_PKT_COMMENT) {
		giterr_set(GITERR_NET, "Invalid HTTP response");
		return t->error = -1;
	} else {
		/* Remove the comment pkt from the list */
		git_vector_remove(&transport->refs, 0);
		git__free(pkt);
	}

	if (git_protocol_detect_caps(git_vector_get(&transport->refs, 0), &transport->caps) < 0)
		return t->error = -1;

cleanup:
	git_buf_free(&request);
	git_buf_clear(&t->buf);

	return ret;
}

static int http_negotiation_step(struct git_transport *transport, void *data, size_t len)
{
	transport_http *t = (transport_http *) transport;
	int ret;

	/* First, send the data as a HTTP POST request */
	if ((ret = do_connect(t)) < 0)
		return -1;

	if (send_request(t, "upload-pack", data, len, 0) < 0)
		return -1;

	/* Then we need to set up the buffer to grab data from the HTTP response */
	setup_gitno_buffer(transport);

	return 0;
}

static int http_close(git_transport *transport)
{
#ifndef GIT_WINHTTP
	if (gitno_ssl_teardown(transport) < 0)
		return -1;

	if (gitno_close(transport->socket) < 0) {
		giterr_set(GITERR_OS, "Failed to close the socket: %s", strerror(errno));
		return -1;
	}
#else
	transport_http *t = (transport_http *) transport;

	if (t->request)
		WinHttpCloseHandle(t->request);
	if (t->connection)
		WinHttpCloseHandle(t->connection);
	if (t->session)
		WinHttpCloseHandle(t->session);
#endif

	transport->connected = 0;

	return 0;
}


static void http_free(git_transport *transport)
{
	transport_http *t = (transport_http *) transport;
	git_vector *refs = &transport->refs;
	git_vector *common = &transport->common;
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
	t->parent.negotiation_step = http_negotiation_step;
	t->parent.close = http_close;
	t->parent.free = http_free;
	t->parent.rpc = 1;

	if (git_vector_init(&t->parent.refs, 16, NULL) < 0) {
		git__free(t);
		return -1;
	}

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

int git_transport_https(git_transport **out)
{
#if defined(GIT_SSL) || defined(GIT_WINHTTP)
	transport_http *t;
	if (git_transport_http((git_transport **)&t) < 0)
		return -1;

	t->parent.use_ssl = 1;
	t->parent.check_cert = 1;
	*out = (git_transport *) t;

	return 0;
#else
	GIT_UNUSED(out);

	giterr_set(GITERR_NET, "HTTPS support not available");
	return -1;
#endif
}
