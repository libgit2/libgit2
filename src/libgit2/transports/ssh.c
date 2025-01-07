/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "backend.h"
#include "ssh.h"
#include "ssh_exec.h"
#include "ssh_libssh2.h"

#include "transports/smart.h"

typedef struct git_ssh__backend {
	int (* subtransport)(
		git_smart_subtransport **out,
		git_transport *owner,
		void *param);

	int (* set_paths)(
		git_smart_subtransport *subtransport,
		const char *cmd_uploadpack,
		const char *cmd_receivepack);
} git_ssh__backend_t;

#if defined(GIT_SSH_LIBSSH2)
static git_ssh__backend_t libssh2_backend = {
	git_smart_subtransport_ssh_libssh2,
	git_smart_subtransport_ssh_libssh2_set_paths
};
#endif

#if defined(GIT_SSH_EXEC)
static git_ssh__backend_t exec_backend = {
	git_smart_subtransport_ssh_exec,
	git_smart_subtransport_ssh_exec_set_paths
};
#endif

static const git_ssh__backend_t *backend = NULL;

static int install_ssh_backend(void *param)
{
	backend = param;
	return 0;
}

static int uninstall_ssh_backend(void *param)
{
	backend = NULL;
	return 0;

	GIT_UNUSED(param);
}

int git_transport_ssh_global_init(void)
{
	int error = 0;

#if defined(GIT_SSH_LIBSSH2)
	error = git_backend__register(
		GIT_FEATURE_SSH,
		"libssh2",
		install_ssh_backend,
		uninstall_ssh_backend,
		&libssh2_backend);
	GIT_ERROR_CHECK_ERROR(error);
#endif

#if defined(GIT_SSH_EXEC)
	error = git_backend__register(
		GIT_FEATURE_SSH,
		"exec",
		install_ssh_backend,
		uninstall_ssh_backend,
		&exec_backend);
	GIT_ERROR_CHECK_ERROR(error);
#endif

	return error;
}

int git_smart_subtransport_ssh(
	git_smart_subtransport **out,
	git_transport *owner,
	void *param)
{
	if (!backend) {
		git_error_set(GIT_ERROR_INVALID, "cannot create SSH transport; no SSH backend is set");
		return -1;
	}

	return backend->subtransport(out, owner, param);
}

static int transport_set_paths(git_transport *t, git_strarray *paths)
{
	transport_smart *smart = (transport_smart *)t;

	if (!backend) {
		GIT_ASSERT(!"cannot create SSH library; no SSH backend is set");
		return -1;
	}

	return backend->set_paths(
		(git_smart_subtransport *)smart->wrapped,
		paths->strings[0],
		paths->strings[1]);
}

int git_transport_ssh_with_paths(
	git_transport **out,
	git_remote *owner,
	void *payload)
{
	git_strarray *paths = (git_strarray *) payload;
	git_transport *transport;
	int error;

	git_smart_subtransport_definition ssh_definition = {
		git_smart_subtransport_ssh,
		0, /* no RPC */
		NULL
	};

	if (paths->count != 2) {
		git_error_set(GIT_ERROR_SSH, "invalid ssh paths, must be two strings");
		return GIT_EINVALIDSPEC;
	}

	if ((error = git_transport_smart(&transport, owner, &ssh_definition)) < 0)
		return error;

	if ((error = transport_set_paths(transport, paths)) < 0)
		return error;

	*out = transport;
	return 0;
}
