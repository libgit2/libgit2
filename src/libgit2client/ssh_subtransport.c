/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <git2.h>
#include <git2/sys/transport.h>
#include <git2client/ssh_subtransport.h>

#include <git2_util.h>

#include "runtime.h"
#include "net.h"
#include "path.h"
#include "futils.h"
#include "process.h"

struct git_ssh_subtransport_stream {
	git_smart_subtransport_stream parent;
};

struct git_ssh_subtransport {
	git_smart_subtransport parent;
	git_transport *owner;

	git_ssh_subtransport_stream *current_stream;

	git_smart_service_t action;
	git_process *process;
};

static int ssh_subtransport_stream_read(
	git_smart_subtransport_stream *s,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	git_ssh_subtransport *transport;
	git_ssh_subtransport_stream *stream = (git_ssh_subtransport_stream *)s;
	ssize_t ret;

	assert(stream && stream->parent.subtransport);

	transport = (git_ssh_subtransport *)stream->parent.subtransport;

	if ((ret = git_process_read(transport->process, buffer, buf_size)) < 0)
		return (int)ret;

	*bytes_read = (size_t)ret;
	return 0;
}

static int ssh_subtransport_stream_write(
        git_smart_subtransport_stream *s,
        const char *buffer,
        size_t len)
{
	git_ssh_subtransport *transport;
	git_ssh_subtransport_stream *stream = (git_ssh_subtransport_stream *)s;
	ssize_t ret;

	assert(stream && stream->parent.subtransport);

	transport = (git_ssh_subtransport *)stream->parent.subtransport;

	while (len > 0) {
		if ((ret = git_process_write(transport->process, buffer, len)) < 0)
			return (int)ret;

		len -= ret;
	}

	return 0;
}

static void ssh_subtransport_stream_free(git_smart_subtransport_stream *s)
{
	git_ssh_subtransport_stream *stream = (git_ssh_subtransport_stream *)s;

	git__free(stream);
}

static int ssh_subtransport_stream_init(
	git_ssh_subtransport_stream **out,
	git_ssh_subtransport *transport)
{
	assert(out);

	*out = git__calloc(sizeof(git_ssh_subtransport_stream), 1);
	GIT_ERROR_CHECK_ALLOC(*out);

	(*out)->parent.subtransport = &transport->parent;
	(*out)->parent.read = ssh_subtransport_stream_read;
	(*out)->parent.write = ssh_subtransport_stream_write;
	(*out)->parent.free = ssh_subtransport_stream_free;

	return 0;
}

GIT_INLINE(int) ensure_transport_state(
	git_ssh_subtransport *transport,
	git_smart_service_t expected)
{
	if (transport->action != expected) {
		git_error_set(GIT_ERROR_NET, "invalid transport state");
		return -1;
	}

	return 0;
}

static int start_ssh(
	git_ssh_subtransport *transport,
	git_smart_service_t action,
	const char *sshpath)
{
	const char *args[4];
	const char *env[] = { "GIT_DIR=" };

	git_process_options process_opts = GIT_PROCESS_OPTIONS_INIT;
	git_net_url url = GIT_NET_URL_INIT;
	git_buf userhost = GIT_BUF_INIT;
	const char *command;
	int error;

	process_opts.capture_in = 1;
	process_opts.capture_out = 1;
	process_opts.capture_err = 1;

	switch (action) {
	case GIT_SERVICE_UPLOADPACK_LS:
		command = "git-upload-pack";
		break;
	case GIT_SERVICE_RECEIVEPACK_LS:
		command = "git-receive-pack";
		break;
	default:
		git_error_set(GIT_ERROR_NET, "invalid action");
		error = -1;
		goto done;
	}

	if ((error = git_net_url_parse_ssh(&url, sshpath)) < 0)
		goto done;

	if (url.username) {
		git_buf_puts(&userhost, url.username);
		git_buf_putc(&userhost, '@');
	}
	git_buf_puts(&userhost, url.host);

	args[0] = "/usr/bin/ssh";
	args[1] = userhost.ptr;
	args[2] = command;
	args[3] = url.path;

	if ((error = git_process_new(&transport->process, args, 4, env, 1, &process_opts)) < 0 ||
	    (error = git_process_start(transport->process)) < 0) {
		git_process_free(transport->process);
		transport->process = NULL;
		goto done;
	}

done:
	git_buf_dispose(&userhost);
	git_net_url_dispose(&url);
	return error;
}

static int ssh_subtransport_action(
	git_smart_subtransport_stream **out,
	git_smart_subtransport *t,
	const char *sshpath,
	git_smart_service_t action)
{
	git_ssh_subtransport *transport = (git_ssh_subtransport *)t;
	git_ssh_subtransport_stream *stream = NULL;
	git_smart_service_t expected;
	int error;

	switch (action) {
	case GIT_SERVICE_UPLOADPACK_LS:
	case GIT_SERVICE_RECEIVEPACK_LS:
		if ((error = ensure_transport_state(transport, 0)) < 0 ||
		    (error = ssh_subtransport_stream_init(&stream, transport)) < 0 ||
		    (error = start_ssh(transport, action, sshpath)) < 0)
		    goto on_error;

		transport->current_stream = stream;
		break;

	case GIT_SERVICE_UPLOADPACK:
	case GIT_SERVICE_RECEIVEPACK:
		expected = (action == GIT_SERVICE_UPLOADPACK) ?
			GIT_SERVICE_UPLOADPACK_LS : GIT_SERVICE_RECEIVEPACK_LS;

		if ((error = ensure_transport_state(transport, expected)) < 0)
			goto on_error;

		break;

	default:
		git_error_set(GIT_ERROR_INVALID, "invalid service request");
		goto on_error;
	}

	transport->action = action;
	*out = &transport->current_stream->parent;

	return 0;

on_error:
	if (stream != NULL)
		ssh_subtransport_stream_free(&stream->parent);
	return -1;
}

static int ssh_subtransport_close(git_smart_subtransport *t)
{
	git_ssh_subtransport *transport = (git_ssh_subtransport *)t;

	if (transport->process) {
		git_process_close(transport->process);
		git_process_free(transport->process);
		transport->process = NULL;
	}

	return 0;
}

static void ssh_subtransport_free(git_smart_subtransport *t)
{
	git_ssh_subtransport *transport = (git_ssh_subtransport *)t;

	git__free(transport);
}

int git_ssh_subtransport_new(
	git_smart_subtransport **out,
	git_transport *owner,
	void *payload)
{
	git_ssh_subtransport *transport;

	GIT_UNUSED(payload);

	transport = git__calloc(sizeof(git_ssh_subtransport), 1);
	GIT_ERROR_CHECK_ALLOC(transport);

	transport->owner = owner;
	transport->parent.action = ssh_subtransport_action;
	transport->parent.close = ssh_subtransport_close;
	transport->parent.free = ssh_subtransport_free;

	*out = (git_smart_subtransport *) transport;
	return 0;
}

static void git_ssh_subtransport_shutdown(void)
{
	git_transport_unregister("ssh");
}

int git_ssh_subtransport_register(void)
{
	static git_smart_subtransport_definition ssh_definition = {
		git_ssh_subtransport_new,
		0,
		NULL
	};

	if (git_transport_register("ssh", git_transport_smart, &ssh_definition) < 0)
		return -1;

	return git_runtime_shutdown_register(git_ssh_subtransport_shutdown);
}
