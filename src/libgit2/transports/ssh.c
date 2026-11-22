/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "ssh.h"

#include "ssh_exec.h"
#include "ssh_libssh2.h"

#include "transports/smart.h"

typedef struct {
	git_transport_smart_stateful parent;
	char *cmd_uploadpack;
	char *cmd_receivepack;
} ssh_transport;

#if defined(GIT_SSH_LIBSSH2) || defined(GIT_SSH_EXEC)

static int ssh_open(
	git_transport_smart_stream **out,
	git_transport_smart_base *transport,
	const char *url,
	const char *cmd)
{
#ifdef GIT_SSH_LIBSSH2
	return git_ssh_libssh2__open(out, transport, url, cmd);
#else
	return git_ssh_exec__open(out, transport, url, cmd);
#endif
}

static int ssh_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	ssh_transport *t = GIT_CONTAINER_OF(transport, ssh_transport, parent.parent.parent);
	const char *cmd;
	int error;

	if ((error = git_transport_smart_stateful__begin_connect(&t->parent, url, direction, connect_opts)) < 0)
		return error;

	if (direction == GIT_DIRECTION_FETCH)
		cmd = t->cmd_uploadpack ? t->cmd_uploadpack : GIT_TRANSPORT_SMART_CMD_UPLOADPACK;
	else
		cmd = t->cmd_receivepack ? t->cmd_receivepack : GIT_TRANSPORT_SMART_CMD_RECEIVEPACK;

	if ((error = ssh_open(&t->parent.stream, &t->parent.parent, t->parent.parent.session->url, cmd)) < 0)
		return error;

	return git_transport_smart_stateful__finish_connect(&t->parent);
}

static void ssh_free(git_transport *transport)
{
	ssh_transport *t = GIT_CONTAINER_OF(transport, ssh_transport, parent.parent.parent);

	git__free(t->cmd_uploadpack);
	git__free(t->cmd_receivepack);

	git_transport_smart_stateful__free(transport);
}

int git_transport_ssh(git_transport **out, git_remote *owner, void *param)
{
	ssh_transport *t;

	GIT_UNUSED(param);

	GIT_ASSERT_ARG(out);

	t = git__calloc(1, sizeof(ssh_transport));
	GIT_ERROR_CHECK_ALLOC(t);

	if (git_transport_smart_stateful__init(&t->parent, owner) < 0) {
		git_transport_smart_stateful__free(&t->parent.parent.parent);
		return -1;
	}

	t->parent.parent.parent.connect = ssh_connect;
	t->parent.parent.parent.free = ssh_free;

	*out = &t->parent.parent.parent;
	return 0;
}

#else

int git_transport_ssh(git_transport **out, git_remote *owner, void *param)
{
	GIT_UNUSED(out);
	GIT_UNUSED(owner);
	GIT_UNUSED(param);

	git_error_set(GIT_ERROR_INVALID, "cannot create SSH transport; library was built without SSH support");
	return -1;
}

#endif

int git_transport_ssh_with_paths(
	git_transport **out,
	git_remote *owner,
	void *payload)
{
	git_strarray *paths = (git_strarray *) payload;
	git_transport *transport;
	ssh_transport *t;
	int error;

	if (paths->count != 2) {
		git_error_set(GIT_ERROR_SSH, "invalid ssh paths, must be two strings");
		return GIT_EINVALIDSPEC;
	}

	if ((error = git_transport_ssh(&transport, owner, NULL)) < 0)
		return error;

	t = GIT_CONTAINER_OF(transport, ssh_transport, parent.parent.parent);

	t->cmd_uploadpack = git__strdup(paths->strings[0]);
	t->cmd_receivepack = git__strdup(paths->strings[1]);

	if (!t->cmd_uploadpack || !t->cmd_receivepack) {
		transport->free(transport);
		return -1;
	}

	*out = transport;
	return 0;
}
