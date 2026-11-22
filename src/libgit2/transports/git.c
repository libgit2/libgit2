/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git.h"

#include "net.h"
#include "stream.h"
#include "streams/socket.h"
#include "smart.h"

static const char prefix_git[] = "git://";

typedef struct {
	git_transport_smart_stream parent;
	git_stream *io;
	const char *cmd;
	char *url;
	unsigned sent_command : 1;
} git_proto_stream;

/*
 * Create a git protocol request.
 *
 * For example: 0035git-upload-pack /libgit2/libgit2\0host=github.com\0
 */
static int gen_proto(git_str *request, const char *cmd, const char *url)
{
	const char *delim, *repo;
	char host[] = "host=";
	size_t len;

	delim = strchr(url, '/');
	if (delim == NULL) {
		git_error_set(GIT_ERROR_NET, "malformed URL");
		return -1;
	}

	repo = delim;
	if (repo[1] == '~')
		++repo;

	delim = strchr(url, ':');
	if (delim == NULL)
		delim = strchr(url, '/');

	len = 4 + strlen(cmd) + 1 + strlen(repo) + 1 + strlen(host) + (delim - url) + 1;

	git_str_grow(request, len);
	git_str_printf(request, "%04x%s %s%c%s",
		(unsigned int)(len & 0x0FFFF), cmd, repo, 0, host);
	git_str_put(request, url, delim - url);
	git_str_putc(request, '\0');

	if (git_str_oom(request))
		return -1;

	return 0;
}

static int send_command(git_proto_stream *s)
{
	git_str request = GIT_STR_INIT;
	int error;

	if ((error = gen_proto(&request, s->cmd, s->url)) < 0)
		goto cleanup;

	if ((error = git_stream__write_full(s->io, request.ptr, request.size, 0)) < 0)
		goto cleanup;

	s->sent_command = 1;

cleanup:
	git_str_dispose(&request);
	return error;
}

static int git_proto_stream_read(
	git_transport_smart_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	git_proto_stream *s = (git_proto_stream *)stream;
	ssize_t ret;
	int error;

	*bytes_read = 0;

	if (!s->sent_command && (error = send_command(s)) < 0)
		return error;

	ret = git_stream_read(s->io, buffer, min(buf_size, INT_MAX));

	if (ret < 0)
		return -1;

	*bytes_read = (size_t)ret;
	return 0;
}

static int git_proto_stream_write(
	git_transport_smart_stream *stream,
	const char *buffer,
	size_t len)
{
	git_proto_stream *s = (git_proto_stream *)stream;
	int error;

	if (!s->sent_command && (error = send_command(s)) < 0)
		return error;

	return git_stream__write_full(s->io, buffer, len, 0);
}

static void git_proto_stream_free(git_transport_smart_stream *stream)
{
	git_proto_stream *s = (git_proto_stream *)stream;

	if (s->io) {
		git_stream_close(s->io);
		git_stream_free(s->io);
	}

	git__free(s->url);
	git__free(s);
}

static int git_proto_stream_alloc(
	git_transport_smart_stream **out,
	const char *url,
	const char *cmd,
	const char *host,
	const char *port)
{
	git_proto_stream *s;

	s = git__calloc(1, sizeof(git_proto_stream));
	GIT_ERROR_CHECK_ALLOC(s);

	s->parent.read = git_proto_stream_read;
	s->parent.write = git_proto_stream_write;
	s->parent.free = git_proto_stream_free;

	s->cmd = cmd;
	s->url = git__strdup(url);

	if (!s->url) {
		git_proto_stream_free(&s->parent);
		return -1;
	}

	if (git_socket_stream_new(&s->io, host, port) < 0) {
		git_proto_stream_free(&s->parent);
		return -1;
	}

	*out = &s->parent;

	GIT_ERROR_CHECK_VERSION(s->io, GIT_STREAM_VERSION, "git_stream");

	return 0;
}

static int git_proto_open(
	git_transport_smart_stream **out,
	const char *url,
	const char *cmd)
{
	git_net_url urldata = GIT_NET_URL_INIT;
	git_transport_smart_stream *stream = NULL;
	const char *stream_url = url;
	int error;

	if (!git__prefixcmp(url, prefix_git))
		stream_url += strlen(prefix_git);

	if ((error = git_net_url_parse(&urldata, url)) < 0)
		return error;

	error = git_proto_stream_alloc(&stream, stream_url, cmd,
		urldata.host, urldata.port ? urldata.port : GIT_DEFAULT_PORT);

	git_net_url_dispose(&urldata);

	if (!error)
		error = git_stream_connect(((git_proto_stream *)stream)->io);

	if (error < 0) {
		if (stream)
			stream->free(stream);

		return error;
	}

	*out = stream;
	return 0;
}

static int git_proto_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	git_transport_smart_stateful *t = GIT_CONTAINER_OF(transport, git_transport_smart_stateful, parent.parent);
	const char *cmd;
	int error;

	if ((error = git_transport_smart_stateful__begin_connect(t, url, direction, connect_opts)) < 0)
		return error;

	cmd = (direction == GIT_DIRECTION_FETCH) ?
		GIT_TRANSPORT_SMART_CMD_UPLOADPACK : GIT_TRANSPORT_SMART_CMD_RECEIVEPACK;

	if ((error = git_proto_open(&t->stream, t->parent.session->url, cmd)) < 0)
		return error;

	return git_transport_smart_stateful__finish_connect(t);
}

int git_transport_git(git_transport **out, git_remote *owner, void *param)
{
	git_transport_smart_stateful *t;

	GIT_UNUSED(param);

	if (!out)
		return -1;

	t = git__calloc(1, sizeof(git_transport_smart_stateful));
	GIT_ERROR_CHECK_ALLOC(t);

	if (git_transport_smart_stateful__init(t, owner) < 0) {
		git_transport_smart_stateful__free(&t->parent.parent);
		return -1;
	}

	t->parent.parent.connect = git_proto_connect;

	*out = &t->parent.parent;
	return 0;
}
