/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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
#include "remote.h"
#include "repository.h"

#include <winhttp.h>
#pragma comment(lib, "winhttp")

/* For UuidCreate */
#pragma comment(lib, "rpcrt4")

#define WIDEN2(s) L ## s
#define WIDEN(s) WIDEN2(s)

#define MAX_CONTENT_TYPE_LEN	100
#define WINHTTP_OPTION_PEERDIST_EXTENSION_STATE	109
#define CACHED_POST_BODY_BUF_SIZE	4096
#define UUID_LENGTH_CCH	32

static const char *prefix_http = "http://";
static const char *prefix_https = "https://";
static const char *upload_pack_service = "upload-pack";
static const char *upload_pack_ls_service_url = "/info/refs?service=git-upload-pack";
static const char *upload_pack_service_url = "/git-upload-pack";
static const char *receive_pack_service = "receive-pack";
static const char *receive_pack_ls_service_url = "/info/refs?service=git-receive-pack";
static const char *receive_pack_service_url = "/git-receive-pack";
static const wchar_t *get_verb = L"GET";
static const wchar_t *post_verb = L"POST";
static const wchar_t *pragma_nocache = L"Pragma: no-cache";
static const wchar_t *transfer_encoding = L"Transfer-Encoding: chunked";
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
	char *chunk_buffer;
	unsigned chunk_buffer_len;
	HANDLE post_body;
	DWORD post_body_len;
	unsigned sent_request : 1,
		received_response : 1,
		chunked : 1;
} winhttp_stream;

typedef struct {
	git_smart_subtransport parent;
	transport_smart *owner;
	const char *path;
	char *host;
	char *port;
	char *user_from_url;
	char *pass_from_url;
	git_cred *cred;
	git_cred *url_cred;
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
	int error = -1, wide_len = 0;

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

	wide = git__malloc(wide_len * sizeof(wchar_t));

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
	char *proxy_url = NULL;
	wchar_t url[GIT_WIN_PATH], ct[MAX_CONTENT_TYPE_LEN];
	wchar_t *types[] = { L"*/*", NULL };
	BOOL peerdist = FALSE;
	int error = -1;

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

	/* Set proxy if necessary */
	if (git_remote__get_http_proxy(t->owner->owner, t->use_ssl, &proxy_url) < 0)
		goto on_error;

	if (proxy_url) {
		WINHTTP_PROXY_INFO proxy_info;
		size_t wide_len;

		git__utf8_to_16(url, GIT_WIN_PATH, proxy_url);

		wide_len = wcslen(url);

		/* Strip any trailing forward slash on the proxy URL;
		 * WinHTTP doesn't like it if one is present */
		if (L'/' == url[wide_len - 1])
			url[wide_len - 1] = L'\0';

		proxy_info.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		proxy_info.lpszProxy = url;
		proxy_info.lpszProxyBypass = NULL;

		if (!WinHttpSetOption(s->request,
			WINHTTP_OPTION_PROXY,
			&proxy_info,
			sizeof(WINHTTP_PROXY_INFO))) {
			giterr_set(GITERR_OS, "Failed to set proxy");
			goto on_error;
		}
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

	/* If no other credentials have been applied and the URL has username and
	 * password, use those */
	if (!t->cred && t->user_from_url && t->pass_from_url) {
		if (!t->url_cred &&
			 git_cred_userpass_plaintext_new(&t->url_cred, t->user_from_url, t->pass_from_url) < 0)
			goto on_error;
		if (apply_basic_credential(s->request, t->url_cred) < 0)
			goto on_error;
	}

	/* We've done everything up to calling WinHttpSendRequest. */

	error = 0;

on_error:
	git__free(proxy_url);
	git_buf_free(&buf);
	return error;
}

static int parse_unauthorized_response(
	HINTERNET request,
	int *allowed_types,
	int *auth_mechanism)
{
	DWORD supported, first, target;

	*allowed_types = 0;
	*auth_mechanism = 0;

	/* WinHttpQueryHeaders() must be called before WinHttpQueryAuthSchemes(). 
	 * We can assume this was already done, since we know we are unauthorized. 
	 */
	if (!WinHttpQueryAuthSchemes(request, &supported, &first, &target)) {
		giterr_set(GITERR_OS, "Failed to parse supported auth schemes"); 
		return -1;
	}

	if (WINHTTP_AUTH_SCHEME_BASIC & supported) {
		*allowed_types |= GIT_CREDTYPE_USERPASS_PLAINTEXT;
		*auth_mechanism = GIT_WINHTTP_AUTH_BASIC;
	}

	return 0;
}

static int write_chunk(HINTERNET request, const char *buffer, size_t len)
{
	DWORD bytes_written;
	git_buf buf = GIT_BUF_INIT;

	/* Chunk header */
	git_buf_printf(&buf, "%X\r\n", len);

	if (git_buf_oom(&buf))
		return -1;

	if (!WinHttpWriteData(request,
		git_buf_cstr(&buf),	(DWORD)git_buf_len(&buf),
		&bytes_written)) {
		git_buf_free(&buf);
		giterr_set(GITERR_OS, "Failed to write chunk header");
		return -1;
	}

	git_buf_free(&buf);

	/* Chunk body */
	if (!WinHttpWriteData(request,
		buffer, (DWORD)len,
		&bytes_written)) {
		giterr_set(GITERR_OS, "Failed to write chunk");
		return -1;
	}

	/* Chunk footer */
	if (!WinHttpWriteData(request,
		"\r\n", 2,
		&bytes_written)) {
		giterr_set(GITERR_OS, "Failed to write chunk footer");
		return -1;
	}

	return 0;
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

	if (!s->received_response) {
		DWORD status_code, status_code_length, content_type_length, bytes_written;
		char expected_content_type_8[MAX_CONTENT_TYPE_LEN];
		wchar_t expected_content_type[MAX_CONTENT_TYPE_LEN], content_type[MAX_CONTENT_TYPE_LEN];

		if (!s->sent_request) {
			if (!WinHttpSendRequest(s->request,
				WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0,
				s->post_body_len, 0)) {
				giterr_set(GITERR_OS, "Failed to send request");
				return -1;
			}

			s->sent_request = 1;
		}

		if (s->chunked) {
			assert(s->verb == post_verb);

			/* Flush, if necessary */
			if (s->chunk_buffer_len > 0 &&
				write_chunk(s->request, s->chunk_buffer, s->chunk_buffer_len) < 0)
				return -1;

			s->chunk_buffer_len = 0;

			/* Write the final chunk. */
			if (!WinHttpWriteData(s->request,
				"0\r\n\r\n", 5,
				&bytes_written)) {
				giterr_set(GITERR_OS, "Failed to write final chunk");
				return -1;
			}
		}
		else if (s->post_body) {
			char *buffer;
			DWORD len = s->post_body_len, bytes_read;

			if (INVALID_SET_FILE_POINTER == SetFilePointer(s->post_body,
					0, 0, FILE_BEGIN) &&
				NO_ERROR != GetLastError()) {
				giterr_set(GITERR_OS, "Failed to reset file pointer");
				return -1;
			}

			buffer = git__malloc(CACHED_POST_BODY_BUF_SIZE);

			while (len > 0) {
				DWORD bytes_written;

				if (!ReadFile(s->post_body, buffer,
					min(CACHED_POST_BODY_BUF_SIZE, len),
					&bytes_read, NULL) ||
					!bytes_read) {
					git__free(buffer);
					giterr_set(GITERR_OS, "Failed to read from temp file");
					return -1;
				}

				if (!WinHttpWriteData(s->request, buffer,
					bytes_read, &bytes_written)) {
					git__free(buffer);
					giterr_set(GITERR_OS, "Failed to write data");
					return -1;
				}

				len -= bytes_read;
				assert(bytes_read == bytes_written);
			}

			git__free(buffer);

			/* Eagerly close the temp file */
			CloseHandle(s->post_body);
			s->post_body = NULL;
		}

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

				if (t->owner->cred_acquire_cb(&t->cred, t->owner->url, t->user_from_url, allowed_types, t->owner->cred_acquire_payload) < 0)
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

static int winhttp_stream_write_single(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
	winhttp_stream *s = (winhttp_stream *)stream;
	winhttp_subtransport *t = OWNING_SUBTRANSPORT(s);
	DWORD bytes_written;

	if (!s->request && winhttp_stream_connect(s) < 0)
		return -1;

	/* This implementation of write permits only a single call. */
	if (s->sent_request) {
		giterr_set(GITERR_NET, "Subtransport configured for only one write");
		return -1;
	}

	if (!WinHttpSendRequest(s->request,
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
		giterr_set(GITERR_OS, "Failed to write data");
		return -1;
	}

	assert((DWORD)len == bytes_written);

	return 0;
}

static int put_uuid_string(LPWSTR buffer, size_t buffer_len_cch)
{
	UUID uuid;
	RPC_STATUS status = UuidCreate(&uuid);
	HRESULT result;

	if (RPC_S_OK != status &&
		RPC_S_UUID_LOCAL_ONLY != status &&
		RPC_S_UUID_NO_ADDRESS != status) {
		giterr_set(GITERR_NET, "Unable to generate name for temp file");
		return -1;
	}

	if (buffer_len_cch < UUID_LENGTH_CCH + 1) {
		giterr_set(GITERR_NET, "Buffer too small for name of temp file");
		return -1;
	}

	result = StringCbPrintfW(
		buffer, buffer_len_cch,
		L"%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
		uuid.Data1, uuid.Data2, uuid.Data3,
		uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3],
		uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);

	if (FAILED(result)) {
		giterr_set(GITERR_OS, "Unable to generate name for temp file");
		return -1;
	}

	return 0;
}

static int get_temp_file(LPWSTR buffer, DWORD buffer_len_cch)
{
	size_t len;

	if (!GetTempPathW(buffer_len_cch, buffer)) {
		giterr_set(GITERR_OS, "Failed to get temp path");
		return -1;
	}

	len = wcslen(buffer);

	if (buffer[len - 1] != '\\' && len < buffer_len_cch)
		buffer[len++] = '\\';

	if (put_uuid_string(&buffer[len], (size_t)buffer_len_cch - len) < 0)
		return -1;

	return 0;
}

static int winhttp_stream_write_buffered(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
	winhttp_stream *s = (winhttp_stream *)stream;
	winhttp_subtransport *t = OWNING_SUBTRANSPORT(s);
	DWORD bytes_written;

	if (!s->request && winhttp_stream_connect(s) < 0)
		return -1;

	/* Buffer the payload, using a temporary file so we delegate
	 * memory management of the data to the operating system. */
	if (!s->post_body) {
		wchar_t temp_path[MAX_PATH + 1];

		if (get_temp_file(temp_path, MAX_PATH + 1) < 0)
			return -1;

		s->post_body = CreateFileW(temp_path,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_DELETE, NULL,
			CREATE_NEW,
			FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_SEQUENTIAL_SCAN,
			NULL);

		if (INVALID_HANDLE_VALUE == s->post_body) {
			s->post_body = NULL;
			giterr_set(GITERR_OS, "Failed to create temporary file");
			return -1;
		}
	}

	if (!WriteFile(s->post_body, buffer, (DWORD)len, &bytes_written, NULL)) {
		giterr_set(GITERR_OS, "Failed to write to temporary file");
		return -1;
	}

	assert((DWORD)len == bytes_written);

	s->post_body_len += bytes_written;

	return 0;
}

static int winhttp_stream_write_chunked(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
	winhttp_stream *s = (winhttp_stream *)stream;
	winhttp_subtransport *t = OWNING_SUBTRANSPORT(s);

	if (!s->request && winhttp_stream_connect(s) < 0)
		return -1;

	if (!s->sent_request) {
		/* Send Transfer-Encoding: chunked header */
		if (!WinHttpAddRequestHeaders(s->request,
			transfer_encoding, (ULONG) -1L,
			WINHTTP_ADDREQ_FLAG_ADD)) {
			giterr_set(GITERR_OS, "Failed to add a header to the request");
			return -1;
		}

		if (!WinHttpSendRequest(s->request,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0,
			WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0)) {
			giterr_set(GITERR_OS, "Failed to send request");
			return -1;
		}

		s->sent_request = 1;
	}

	if (len > CACHED_POST_BODY_BUF_SIZE) {
		/* Flush, if necessary */
		if (s->chunk_buffer_len > 0) {
			if (write_chunk(s->request, s->chunk_buffer, s->chunk_buffer_len) < 0)
				return -1;

			s->chunk_buffer_len = 0;
		}

		/* Write chunk directly */
		if (write_chunk(s->request, buffer, len) < 0)
			return -1;
	}
	else {
		/* Append as much to the buffer as we can */
		int count = min(CACHED_POST_BODY_BUF_SIZE - s->chunk_buffer_len, (int)len);

		if (!s->chunk_buffer)
			s->chunk_buffer = git__malloc(CACHED_POST_BODY_BUF_SIZE);

		memcpy(s->chunk_buffer + s->chunk_buffer_len, buffer, count);
		s->chunk_buffer_len += count;
		buffer += count;
		len -= count;

		/* Is the buffer full? If so, then flush */
		if (CACHED_POST_BODY_BUF_SIZE == s->chunk_buffer_len) {
			if (write_chunk(s->request, s->chunk_buffer, s->chunk_buffer_len) < 0)
				return -1;

			s->chunk_buffer_len = 0;

			/* Is there any remaining data from the source? */
			if (len > 0) {
				memcpy(s->chunk_buffer, buffer, len);
				s->chunk_buffer_len = (unsigned int)len;
			}
		}
	}

	return 0;
}

static void winhttp_stream_free(git_smart_subtransport_stream *stream)
{
	winhttp_stream *s = (winhttp_stream *)stream;

	if (s->chunk_buffer) {
		git__free(s->chunk_buffer);
		s->chunk_buffer = NULL;
	}

	if (s->post_body) {
		CloseHandle(s->post_body);
		s->post_body = NULL;
	}

	if (s->request) {
		WinHttpCloseHandle(s->request);
		s->request = NULL;
	}

	git__free(s);
}

static int winhttp_stream_alloc(winhttp_subtransport *t, winhttp_stream **stream)
{
	winhttp_stream *s;

	if (!stream)
		return -1;

	s = git__calloc(sizeof(winhttp_stream), 1);
	GITERR_CHECK_ALLOC(s);

	s->parent.subtransport = &t->parent;
	s->parent.read = winhttp_stream_read;
	s->parent.write = winhttp_stream_write_single;
	s->parent.free = winhttp_stream_free;

	*stream = s;

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

	if ((ret = gitno_extract_url_parts(&t->host, &t->port, &t->user_from_url,
					&t->pass_from_url, url, default_port)) < 0)
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
	winhttp_stream *s)
{
	s->service = upload_pack_service;
	s->service_url = upload_pack_ls_service_url;
	s->verb = get_verb;

	return 0;
}

static int winhttp_uploadpack(
	winhttp_subtransport *t,
	winhttp_stream *s)
{
	s->service = upload_pack_service;
	s->service_url = upload_pack_service_url;
	s->verb = post_verb;

	return 0;
}

static int winhttp_receivepack_ls(
	winhttp_subtransport *t,
	winhttp_stream *s)
{
	s->service = receive_pack_service;
	s->service_url = receive_pack_ls_service_url;
	s->verb = get_verb;

	return 0;
}

static int winhttp_receivepack(
	winhttp_subtransport *t,
	winhttp_stream *s)
{
	/* WinHTTP only supports Transfer-Encoding: chunked
	 * on Windows Vista (NT 6.0) and higher. */
	s->chunked = LOBYTE(LOWORD(GetVersion())) >= 6;

	if (s->chunked)
		s->parent.write = winhttp_stream_write_chunked;
	else
		s->parent.write = winhttp_stream_write_buffered;

	s->service = receive_pack_service;
	s->service_url = receive_pack_service_url;
	s->verb = post_verb;

	return 0;
}

static int winhttp_action(
	git_smart_subtransport_stream **stream,
	git_smart_subtransport *subtransport,
	const char *url,
	git_smart_service_t action)
{
	winhttp_subtransport *t = (winhttp_subtransport *)subtransport;
	winhttp_stream *s;
	int ret = -1;

	if (!t->connection &&
		winhttp_connect(t, url) < 0)
		return -1;

	if (winhttp_stream_alloc(t, &s) < 0)
		return -1;

	if (!stream)
		return -1;

	switch (action)
	{
		case GIT_SERVICE_UPLOADPACK_LS:
			ret = winhttp_uploadpack_ls(t, s);
			break;

		case GIT_SERVICE_UPLOADPACK:
			ret = winhttp_uploadpack(t, s);
			break;

		case GIT_SERVICE_RECEIVEPACK_LS:
			ret = winhttp_receivepack_ls(t, s);
			break;

		case GIT_SERVICE_RECEIVEPACK:
			ret = winhttp_receivepack(t, s);
			break;

		default:
			assert(0);
	}

	if (!ret)
		*stream = &s->parent;

	return ret;
}

static int winhttp_close(git_smart_subtransport *subtransport)
{
	winhttp_subtransport *t = (winhttp_subtransport *)subtransport;
	int ret = 0;

	if (t->host) {
		git__free(t->host);
		t->host = NULL;
	}

	if (t->port) {
		git__free(t->port);
		t->port = NULL;
	}

	if (t->user_from_url) {
		git__free(t->user_from_url);
		t->user_from_url = NULL;
	}

	if (t->pass_from_url) {
		git__free(t->pass_from_url);
		t->pass_from_url = NULL;
	}

	if (t->cred) {
		t->cred->free(t->cred);
		t->cred = NULL;
	}

	if (t->url_cred) {
		t->url_cred->free(t->url_cred);
		t->url_cred = NULL;
	}

	if (t->connection) {
		if (!WinHttpCloseHandle(t->connection)) {
			giterr_set(GITERR_OS, "Unable to close connection");
			ret = -1;
		}

		t->connection = NULL;
	}

	if (t->session) {
		if (!WinHttpCloseHandle(t->session)) {
			giterr_set(GITERR_OS, "Unable to close session");
			ret = -1;
		}

		t->session = NULL;
	}

	return ret;
}

static void winhttp_free(git_smart_subtransport *subtransport)
{
	winhttp_subtransport *t = (winhttp_subtransport *)subtransport;

	winhttp_close(subtransport);

	git__free(t);
}

int git_smart_subtransport_http(git_smart_subtransport **out, git_transport *owner)
{
	winhttp_subtransport *t;

	if (!out)
		return -1;

	t = git__calloc(sizeof(winhttp_subtransport), 1);
	GITERR_CHECK_ALLOC(t);

	t->owner = (transport_smart *)owner;
	t->parent.action = winhttp_action;
	t->parent.close = winhttp_close;
	t->parent.free = winhttp_free;

	*out = (git_smart_subtransport *) t;
	return 0;
}

#endif /* GIT_WINHTTP */
