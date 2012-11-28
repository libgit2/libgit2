/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifdef GIT_WINHTTP

#include "git2.h"
#include "git2/transport.h"
#include "buffer.h"
#include "posix.h"
#include "netops.h"
#include "smart.h"

#include <winhttp.h>
#pragma comment(lib, "winhttp")

#define WIDEN2(s) L ## s
#define WIDEN(s) WIDEN2(s)

#define MAX_CONTENT_TYPE_LEN	100
#define WINHTTP_OPTION_PEERDIST_EXTENSION_STATE	109

static const char *prefix_http = "http://";
static const char *prefix_https = "https://";
static const char *upload_pack_service = "upload-pack";
static const char *upload_pack_ls_service_url = "/info/refs?service=git-upload-pack";
static const char *upload_pack_service_url = "/git-upload-pack";
static const wchar_t *get_verb = L"GET";
static const wchar_t *post_verb = L"POST";
static const wchar_t *basic_authtype = L"Basic";
static const wchar_t *pragma_nocache = L"Pragma: no-cache";
static const int no_check_cert_flags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
	SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
	SECURITY_FLAG_IGNORE_UNKNOWN_CA;

#define OWNING_SUBTRANSPORT(s) ((winhttp_subtransport *)(s)->parent.subtransport)

typedef enum {
	GIT_WINHTTP_AUTH_BASIC = 1,
} winhttp_authmechanism_t;

typedef struct {
	git_smart_subtransport_stream parent;
	const char *service;
	const char *service_url;
	const wchar_t *verb;
	HINTERNET request;
	unsigned sent_request : 1,
		received_response : 1;
} winhttp_stream;

typedef struct {
	git_smart_subtransport parent;
	transport_smart *owner;
	const char *path;
	char *host;
	char *port;
	git_cred *cred;
	int auth_mechanism;
	HINTERNET session;
	HINTERNET connection;
	unsigned use_ssl : 1;
} winhttp_subtransport;

static int apply_basic_credential(HINTERNET request, git_cred *cred)
{
	git_cred_userpass_plaintext *c = (git_cred_userpass_plaintext *)cred;
	git_buf buf = GIT_BUF_INIT, raw = GIT_BUF_INIT;
	wchar_t *wide = NULL;
	int error = -1, wide_len;

	git_buf_printf(&raw, "%s:%s", c->username, c->password);

	if (git_buf_oom(&raw) ||
		git_buf_puts(&buf, "Authorization: Basic ") < 0 ||
		git_buf_put_base64(&buf, git_buf_cstr(&raw), raw.size) < 0)
		goto on_error;

	wide_len = MultiByteToWideChar(CP_UTF8,	MB_ERR_INVALID_CHARS,
		git_buf_cstr(&buf),	-1, NULL, 0);

	if (!wide_len) {
		giterr_set(GITERR_OS, "Failed to measure string for wide conversion");
		goto on_error;
	}

	wide = (wchar_t *)git__malloc(wide_len * sizeof(wchar_t));

	if (!wide)
		goto on_error;

	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		git_buf_cstr(&buf), -1, wide, wide_len)) {
		giterr_set(GITERR_OS, "Failed to convert string to wide form");
		goto on_error;
	}

	if (!WinHttpAddRequestHeaders(request, wide, (ULONG) -1L, WINHTTP_ADDREQ_FLAG_ADD)) {
		giterr_set(GITERR_OS, "Failed to add a header to the request");
		goto on_error;
	}

	error = 0;

on_error:
	/* We were dealing with plaintext passwords, so clean up after ourselves a bit. */
	if (wide)
		memset(wide, 0x0, wide_len * sizeof(wchar_t));

	if (buf.size)
		memset(buf.ptr, 0x0, buf.size);

	if (raw.size)
		memset(raw.ptr, 0x0, raw.size);

	git__free(wide);
	git_buf_free(&buf);
	git_buf_free(&raw);
	return error;
}

static int winhttp_stream_connect(winhttp_stream *s)
{
	winhttp_subtransport *t = OWNING_SUBTRANSPORT(s);
	git_buf buf = GIT_BUF_INIT;
	wchar_t url[GIT_WIN_PATH], ct[MAX_CONTENT_TYPE_LEN];
	wchar_t *types[] = { L"*/*", NULL };
	BOOL peerdist = FALSE;

	/* Prepare URL */
	git_buf_printf(&buf, "%s%s", t->path, s->service_url);

	if (git_buf_oom(&buf))
		return -1;

	git__utf8_to_16(url, GIT_WIN_PATH, git_buf_cstr(&buf));

	/* Establish request */
	s->request = WinHttpOpenRequest(
			t->connection,
			s->verb,
			url,
			NULL,
			WINHTTP_NO_REFERER,
			types,
			t->use_ssl ? WINHTTP_FLAG_SECURE : 0);

	if (!s->request) {
		giterr_set(GITERR_OS, "Failed to open request");
		goto on_error;
	}

	/* Strip unwanted headers (X-P2P-PeerDist, X-P2P-PeerDistEx) that WinHTTP
	 * adds itself. This option may not be supported by the underlying
	 * platform, so we do not error-check it */
	WinHttpSetOption(s->request,
		WINHTTP_OPTION_PEERDIST_EXTENSION_STATE,
		&peerdist,
		sizeof(peerdist));

	/* Send Pragma: no-cache header */
	if (!WinHttpAddRequestHeaders(s->request, pragma_nocache, (ULONG) -1L, WINHTTP_ADDREQ_FLAG_ADD)) {
		giterr_set(GITERR_OS, "Failed to add a header to the request");
		goto on_error;
	}

	/* Send Content-Type header -- only necessary on a POST */
	if (post_verb == s->verb) {
		git_buf_clear(&buf);
		if (git_buf_printf(&buf, "Content-Type: application/x-git-%s-request", s->service) < 0)
			goto on_error;

		git__utf8_to_16(ct, MAX_CONTENT_TYPE_LEN, git_buf_cstr(&buf));

		if (!WinHttpAddRequestHeaders(s->request, ct, (ULONG) -1L, WINHTTP_ADDREQ_FLAG_ADD)) {
			giterr_set(GITERR_OS, "Failed to add a header to the request");
			goto on_error;
		}
	}

	/* If requested, disable certificate validation */
	if (t->use_ssl) {
		int flags;

		if (t->owner->parent.read_flags(&t->owner->parent, &flags) < 0)
			goto on_error;

		if ((GIT_TRANSPORTFLAGS_NO_CHECK_CERT & flags) &&
			!WinHttpSetOption(s->request, WINHTTP_OPTION_SECURITY_FLAGS,
			(LPVOID)&no_check_cert_flags, sizeof(no_check_cert_flags))) {
			giterr_set(GITERR_OS, "Failed to set options to ignore cert errors");
			goto on_error;
		}
	}

	/* If we have a credential on the subtransport, apply it to the request */
	if (t->cred &&
		t->cred->credtype == GIT_CREDTYPE_USERPASS_PLAINTEXT &&
		t->auth_mechanism == GIT_WINHTTP_AUTH_BASIC &&
		apply_basic_credential(s->request, t->cred) < 0)
		goto on_error;

	/* We've done everything up to calling WinHttpSendRequest. */

	return 0;

on_error:
	git_buf_free(&buf);
	return -1;
}

static int parse_unauthorized_response(
	HINTERNET request,
	int *allowed_types,
	int *auth_mechanism)
{
	DWORD index, buf_size, last_error;
	int error = 0;
	wchar_t *buf = NULL;

	*allowed_types = 0;

	for (index = 0; ; index++) {
		/* Make a first call to ask for the size of the buffer to allocate
		 * to hold the WWW-Authenticate header */
		if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_WWW_AUTHENTICATE,
			WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
			&buf_size, &index))
		{
			last_error = GetLastError();

			if (ERROR_WINHTTP_HEADER_NOT_FOUND == last_error) {
				/* End of enumeration */
				break;
			} else if (ERROR_INSUFFICIENT_BUFFER == last_error) {
				git__free(buf);
				buf = (wchar_t *)git__malloc(buf_size);

				if (!buf) {
					error = -1;
					break;
				}
			} else {
				giterr_set(GITERR_OS, "Failed to read WWW-Authenticate header");
				error = -1;
				break;
			}
		}

		/* Actually receive the data into our now-allocated buffer */
		if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_WWW_AUTHENTICATE,
			WINHTTP_HEADER_NAME_BY_INDEX, buf,
			&buf_size, &index)) {
			giterr_set(GITERR_OS, "Failed to read WWW-Authenticate header");
			error = -1;
			break;
		}

		if (!wcsncmp(buf, basic_authtype, 5) &&
			(buf[5] == L'\0' || buf[5] == L' ')) {
			*allowed_types |= GIT_CREDTYPE_USERPASS_PLAINTEXT;
			*auth_mechanism = GIT_WINHTTP_AUTH_BASIC;
		}
	}

	git__free(buf);
	return error;
}

static int winhttp_stream_read(
	git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	winhttp_stream *s = (winhttp_stream *)stream;
	winhttp_subtransport *t = OWNING_SUBTRANSPORT(s);
	DWORD dw_bytes_read;

replay:
	/* Connect if necessary */
	if (!s->request && winhttp_stream_connect(s) < 0)
		return -1;

	if (!s->sent_request &&
		!WinHttpSendRequest(s->request,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0,
			0, 0)) {
		giterr_set(GITERR_OS, "Failed to send request");
		return -1;
	}

	s->sent_request = 1;

	if (!s->received_response) {
		DWORD status_code, status_code_length, content_type_length;
		char expected_content_type_8[MAX_CONTENT_TYPE_LEN];
		wchar_t expected_content_type[MAX_CONTENT_TYPE_LEN], content_type[MAX_CONTENT_TYPE_LEN];

		if (!WinHttpReceiveResponse(s->request, 0)) {
			giterr_set(GITERR_OS, "Failed to receive response");
			return -1;
		}

		/* Verify that we got a 200 back */
		status_code_length = sizeof(status_code);

		if (!WinHttpQueryHeaders(s->request,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&status_code, &status_code_length,
			WINHTTP_NO_HEADER_INDEX)) {
				giterr_set(GITERR_OS, "Failed to retreive status code");
				return -1;
		}

		/* Handle authentication failures */
		if (HTTP_STATUS_DENIED == status_code &&
			get_verb == s->verb && t->owner->cred_acquire_cb) {
			int allowed_types;

			if (parse_unauthorized_response(s->request, &allowed_types, &t->auth_mechanism) < 0)
				return -1;

			if (allowed_types &&
				(!t->cred || 0 == (t->cred->credtype & allowed_types))) {

				if (t->owner->cred_acquire_cb(&t->cred, t->owner->url, allowed_types) < 0)
					return -1;

				assert(t->cred);

				WinHttpCloseHandle(s->request);
				s->request = NULL;
				s->sent_request = 0;

				/* Successfully acquired a credential */
				goto replay;
			}
		}

		if (HTTP_STATUS_OK != status_code) {
			giterr_set(GITERR_NET, "Request failed with status code: %d", status_code);
			return -1;
		}

		/* Verify that we got the correct content-type back */
		if (post_verb == s->verb)
			snprintf(expected_content_type_8, MAX_CONTENT_TYPE_LEN, "application/x-git-%s-result", s->service);
		else
			snprintf(expected_content_type_8, MAX_CONTENT_TYPE_LEN, "application/x-git-%s-advertisement", s->service);

		git__utf8_to_16(expected_content_type, MAX_CONTENT_TYPE_LEN, expected_content_type_8);
		content_type_length = sizeof(content_type);

		if (!WinHttpQueryHeaders(s->request,
			WINHTTP_QUERY_CONTENT_TYPE,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&content_type, &content_type_length,
			WINHTTP_NO_HEADER_INDEX)) {
				giterr_set(GITERR_OS, "Failed to retrieve response content-type");
				return -1;
		}

		if (wcscmp(expected_content_type, content_type)) {
			giterr_set(GITERR_NET, "Received unexpected content-type");
			return -1;
		}

		s->received_response = 1;
	}

	if (!WinHttpReadData(s->request,
		(LPVOID)buffer,
		(DWORD)buf_size,
		&dw_bytes_read))
	{
		giterr_set(GITERR_OS, "Failed to read data");
		return -1;
	}

	*bytes_read = dw_bytes_read;

	return 0;
}

static int winhttp_stream_write(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
	winhttp_stream *s = (winhttp_stream *)stream;
	winhttp_subtransport *t = OWNING_SUBTRANSPORT(s);
	DWORD bytes_written;

	if (!s->request && winhttp_stream_connect(s) < 0)
		return -1;

	/* Since we have to write the Content-Length header up front, we're
	 * basically limited to a single call to write() per request. */
	if (!s->sent_request &&
		!WinHttpSendRequest(s->request,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0,
			(DWORD)len, 0)) {
		giterr_set(GITERR_OS, "Failed to send request");
		return -1;
	}

	s->sent_request = 1;

	if (!WinHttpWriteData(s->request,
			(LPCVOID)buffer,
			(DWORD)len,
			&bytes_written)) {
		giterr_set(GITERR_OS, "Failed to send request");
		return -1;
	}

	assert((DWORD)len == bytes_written);

	return 0;
}

static void winhttp_stream_free(git_smart_subtransport_stream *stream)
{
	winhttp_stream *s = (winhttp_stream *)stream;

	if (s->request) {
		WinHttpCloseHandle(s->request);
		s->request = NULL;
	}

	git__free(s);
}

static int winhttp_stream_alloc(winhttp_subtransport *t, git_smart_subtransport_stream **stream)
{
	winhttp_stream *s;

	if (!stream)
		return -1;

	s = (winhttp_stream *)git__calloc(sizeof(winhttp_stream), 1);
	GITERR_CHECK_ALLOC(s);

	s->parent.subtransport = &t->parent;
	s->parent.read = winhttp_stream_read;
	s->parent.write = winhttp_stream_write;
	s->parent.free = winhttp_stream_free;

	*stream = (git_smart_subtransport_stream *)s;
	return 0;
}

static int winhttp_connect(
	winhttp_subtransport *t,
	const char *url)
{
	wchar_t *ua = L"git/1.0 (libgit2 " WIDEN(LIBGIT2_VERSION) L")";
	wchar_t host[GIT_WIN_PATH];
	int32_t port;
	const char *default_port;
	int ret;

	if (!git__prefixcmp(url, prefix_http)) {
		url = url + strlen(prefix_http);
		default_port = "80";
	}

	if (!git__prefixcmp(url, prefix_https)) {
		url += strlen(prefix_https);
		default_port = "443";
		t->use_ssl = 1;
	}

	if ((ret = gitno_extract_host_and_port(&t->host, &t->port, url, default_port)) < 0)
		return ret;

	t->path = strchr(url, '/');

	/* Prepare port */
	if (git__strtol32(&port, t->port, NULL, 10) < 0)
		return -1;

	/* Prepare host */
	git__utf8_to_16(host, GIT_WIN_PATH, t->host);

	/* Establish session */
	t->session = WinHttpOpen(
			ua,
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0);

	if (!t->session) {
		giterr_set(GITERR_OS, "Failed to init WinHTTP");
		return -1;
	}
	
	/* Establish connection */
	t->connection = WinHttpConnect(
			t->session,
			host,
			port,
			0);

	if (!t->connection) {
		giterr_set(GITERR_OS, "Failed to connect to host");
		return -1;
	}

	return 0;
}

static int winhttp_uploadpack_ls(
	winhttp_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	winhttp_stream *s;

	if (!t->connection &&
		winhttp_connect(t, url) < 0)
		return -1;

	if (winhttp_stream_alloc(t, stream) < 0)
		return -1;

	s = (winhttp_stream *)*stream;

	s->service = upload_pack_service;
	s->service_url = upload_pack_ls_service_url;
	s->verb = get_verb;

	return 0;
}

static int winhttp_uploadpack(
	winhttp_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	winhttp_stream *s;

	if (!t->connection &&
		winhttp_connect(t, url) < 0)
		return -1;

	if (winhttp_stream_alloc(t, stream) < 0)
		return -1;

	s = (winhttp_stream *)*stream;

	s->service = upload_pack_service;
	s->service_url = upload_pack_service_url;
	s->verb = post_verb;

	return 0;
}

static int winhttp_action(
	git_smart_subtransport_stream **stream,
	git_smart_subtransport *smart_transport,
	const char *url,
	git_smart_service_t action)
{
	winhttp_subtransport *t = (winhttp_subtransport *)smart_transport;

	if (!stream)
		return -1;

	switch (action)
	{
		case GIT_SERVICE_UPLOADPACK_LS:
			return winhttp_uploadpack_ls(t, url, stream);

		case GIT_SERVICE_UPLOADPACK:
			return winhttp_uploadpack(t, url, stream);
	}

	*stream = NULL;
	return -1;
}

static void winhttp_free(git_smart_subtransport *smart_transport)
{
	winhttp_subtransport *t = (winhttp_subtransport *) smart_transport;
	
	git__free(t->host);
	git__free(t->port);

	if (t->cred) {
		t->cred->free(t->cred);
		t->cred = NULL;
	}

	if (t->connection) {
		WinHttpCloseHandle(t->connection);
		t->connection = NULL;
	}

	if (t->session) {
		WinHttpCloseHandle(t->session);
		t->session = NULL;
	}

	git__free(t);
}

int git_smart_subtransport_http(git_smart_subtransport **out, git_transport *owner)
{
	winhttp_subtransport *t;

	if (!out)
		return -1;

	t = (winhttp_subtransport *)git__calloc(sizeof(winhttp_subtransport), 1);
	GITERR_CHECK_ALLOC(t);

	t->owner = (transport_smart *)owner;
	t->parent.action = winhttp_action;
	t->parent.free = winhttp_free;

	*out = (git_smart_subtransport *) t;
	return 0;
}

#endif /* GIT_WINHTTP */
