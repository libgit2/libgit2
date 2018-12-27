/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "netops.h"

#include <ctype.h>
#include "git2/errors.h"

#include "posix.h"
#include "buffer.h"
#include "http_parser.h"
#include "global.h"

int gitno_recv(gitno_buffer *buf)
{
	return buf->recv(buf);
}

void gitno_buffer_setup_callback(
	gitno_buffer *buf,
	char *data,
	size_t len,
	int (*recv)(gitno_buffer *buf), void *cb_data)
{
	memset(data, 0x0, len);
	buf->data = data;
	buf->len = len;
	buf->offset = 0;
	buf->recv = recv;
	buf->cb_data = cb_data;
}

static int recv_stream(gitno_buffer *buf)
{
	git_stream *io = (git_stream *) buf->cb_data;
	int ret;

	ret = git_stream_read(io, buf->data + buf->offset, buf->len - buf->offset);
	if (ret < 0)
		return -1;

	buf->offset += ret;
	return ret;
}

void gitno_buffer_setup_fromstream(git_stream *st, gitno_buffer *buf, char *data, size_t len)
{
	memset(data, 0x0, len);
	buf->data = data;
	buf->len = len;
	buf->offset = 0;
	buf->recv = recv_stream;
	buf->cb_data = st;
}

/* Consume up to ptr and move the rest of the buffer to the beginning */
void gitno_consume(gitno_buffer *buf, const char *ptr)
{
	size_t consumed;

	assert(ptr - buf->data >= 0);
	assert(ptr - buf->data <= (int) buf->len);

	consumed = ptr - buf->data;

	memmove(buf->data, ptr, buf->offset - consumed);
	memset(buf->data + buf->offset, 0x0, buf->len - buf->offset);
	buf->offset -= consumed;
}

/* Consume const bytes and move the rest of the buffer to the beginning */
void gitno_consume_n(gitno_buffer *buf, size_t cons)
{
	memmove(buf->data, buf->data + cons, buf->len - buf->offset);
	memset(buf->data + cons, 0x0, buf->len - buf->offset);
	buf->offset -= cons;
}

/* Match host names according to RFC 2818 rules */
int gitno__match_host(const char *pattern, const char *host)
{
	for (;;) {
		char c = git__tolower(*pattern++);

		if (c == '\0')
			return *host ? -1 : 0;

		if (c == '*') {
			c = *pattern;
			/* '*' at the end matches everything left */
			if (c == '\0')
				return 0;

	/*
	 * We've found a pattern, so move towards the next matching
	 * char. The '.' is handled specially because wildcards aren't
	 * allowed to cross subdomains.
	 */

			while(*host) {
				char h = git__tolower(*host);
				if (c == h)
					return gitno__match_host(pattern, host++);
				if (h == '.')
					return gitno__match_host(pattern, host);
				host++;
			}
			return -1;
		}

		if (c != git__tolower(*host++))
			return -1;
	}

	return -1;
}

static const char *default_port_http = "80";
static const char *default_port_https = "443";

const char *gitno__default_port(
	gitno_connection_data *data)
{
	return data->use_ssl ? default_port_https : default_port_http;
}

static const char *prefix_http = "http://";
static const char *prefix_https = "https://";

int gitno_connection_data_from_url(
		gitno_connection_data *data,
		const char *url,
		const char *service_suffix)
{
	int error = -1;
	const char *default_port = NULL, *path_search_start = NULL;
	char *original_host = NULL;

	/* service_suffix is optional */
	assert(data && url);

	/* Save these for comparison later */
	original_host = data->host;
	data->host = NULL;
	gitno_connection_data_free_ptrs(data);

	if (!git__prefixcmp(url, prefix_http)) {
		path_search_start = url + strlen(prefix_http);
		default_port = default_port_http;

		if (data->use_ssl) {
			git_error_set(GIT_ERROR_NET, "redirect from HTTPS to HTTP is not allowed");
			goto cleanup;
		}
	} else if (!git__prefixcmp(url, prefix_https)) {
		path_search_start = url + strlen(prefix_https);
		default_port = default_port_https;
		data->use_ssl = true;
	} else if (url[0] == '/')
		default_port = gitno__default_port(data);

	if (!default_port) {
		git_error_set(GIT_ERROR_NET, "unrecognized URL prefix");
		goto cleanup;
	}

	error = gitno_extract_url_parts(
		&data->host, &data->port, &data->path, &data->user, &data->pass,
		url, default_port);

	if (url[0] == '/') {
		/* Relative redirect; reuse original host name and port */
		path_search_start = url;
		git__free(data->host);
		data->host = original_host;
		original_host = NULL;
	}

	if (!error) {
		const char *path = strchr(path_search_start, '/');
		size_t pathlen = strlen(path);
		size_t suffixlen = service_suffix ? strlen(service_suffix) : 0;

		if (suffixlen &&
		    !memcmp(path + pathlen - suffixlen, service_suffix, suffixlen)) {
			git__free(data->path);
			data->path = git__strndup(path, pathlen - suffixlen);
		} else {
			git__free(data->path);
			data->path = git__strdup(path);
		}

		/* Check for errors in the resulting data */
		if (original_host && url[0] != '/' && strcmp(original_host, data->host)) {
			git_error_set(GIT_ERROR_NET, "cross host redirect not allowed");
			error = -1;
		}
	}

cleanup:
	if (original_host) git__free(original_host);
	return error;
}

void gitno_connection_data_free_ptrs(gitno_connection_data *d)
{
	git__free(d->host); d->host = NULL;
	git__free(d->port); d->port = NULL;
	git__free(d->path); d->path = NULL;
	git__free(d->user); d->user = NULL;
	git__free(d->pass); d->pass = NULL;
}

int gitno_extract_url_parts(
	char **host_out,
	char **port_out,
	char **path_out,
	char **username_out,
	char **password_out,
	const char *url,
	const char *default_port)
{
	struct http_parser_url u = {0};
	bool has_host, has_port, has_path, has_userinfo;
	git_buf host = GIT_BUF_INIT,
		port = GIT_BUF_INIT,
		path = GIT_BUF_INIT,
		username = GIT_BUF_INIT,
		password = GIT_BUF_INIT;
	int error = 0;

	if (http_parser_parse_url(url, strlen(url), false, &u)) {
		git_error_set(GIT_ERROR_NET, "malformed URL '%s'", url);
		error = GIT_EINVALIDSPEC;
		goto done;
	}

	has_host = !!(u.field_set & (1 << UF_HOST));
	has_port = !!(u.field_set & (1 << UF_PORT));
	has_path = !!(u.field_set & (1 << UF_PATH));
	has_userinfo = !!(u.field_set & (1 << UF_USERINFO));

	if (has_host) {
		const char *url_host = url + u.field_data[UF_HOST].off;
		size_t url_host_len = u.field_data[UF_HOST].len;
		git_buf_decode_percent(&host, url_host, url_host_len);
	}

	if (has_port) {
		const char *url_port = url + u.field_data[UF_PORT].off;
		size_t url_port_len = u.field_data[UF_PORT].len;
		git_buf_put(&port, url_port, url_port_len);
	} else {
		git_buf_puts(&port, default_port);
	}

	if (has_path && path_out) {
		const char *url_path = url + u.field_data[UF_PATH].off;
		size_t url_path_len = u.field_data[UF_PATH].len;
		git_buf_decode_percent(&path, url_path, url_path_len);
	} else if (path_out) {
		git_error_set(GIT_ERROR_NET, "invalid url, missing path");
		error = GIT_EINVALIDSPEC;
		goto done;
	}

	if (has_userinfo) {
		const char *url_userinfo = url + u.field_data[UF_USERINFO].off;
		size_t url_userinfo_len = u.field_data[UF_USERINFO].len;
		const char *colon = memchr(url_userinfo, ':', url_userinfo_len);

		if (colon) {
			const char *url_username = url_userinfo;
			size_t url_username_len = colon - url_userinfo;
			const char *url_password = colon + 1;
			size_t url_password_len = url_userinfo_len - (url_username_len + 1);

			git_buf_decode_percent(&username, url_username, url_username_len);
			git_buf_decode_percent(&password, url_password, url_password_len);
		} else {
			git_buf_decode_percent(&username, url_userinfo, url_userinfo_len);
		}
	}

	if (git_buf_oom(&host) ||
		git_buf_oom(&port) ||
		git_buf_oom(&path) ||
		git_buf_oom(&username) ||
		git_buf_oom(&password))
		return -1;

	*host_out = git_buf_detach(&host);
	*port_out = git_buf_detach(&port);
	if (path_out)
		*path_out = git_buf_detach(&path);
	*username_out = git_buf_detach(&username);
	*password_out = git_buf_detach(&password);

done:
	git_buf_dispose(&host);
	git_buf_dispose(&port);
	git_buf_dispose(&path);
	git_buf_dispose(&username);
	git_buf_dispose(&password);
	return error;
}
