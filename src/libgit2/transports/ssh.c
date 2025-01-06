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

/*
 * Default SSH backend. If both backends are compiled in,
 * libssh2 takes precedence over exec for backwards compatibility.
 */
#if defined(GIT_SSH_LIBSSH2)
	#define DEFAULT_BACKEND_NAME "libssh2"
#elif defined(GIT_SSH_EXEC)
	#define DEFAULT_BACKEND_NAME "exec"
#else
	#define DEFAULT_BACKEND_NAME ""
#endif

typedef struct git_ssh__backend {
	const char *name;

	int (* subtransport)(
		git_smart_subtransport **out,
		git_transport *owner,
		void *param);

	int (* set_paths)(
		git_smart_subtransport *subtransport,
		const char *cmd_uploadpack,
		const char *cmd_receivepack);
} git_ssh__backend_t;

static const git_ssh__backend_t backend_table[] = {
#if defined(GIT_SSH_LIBSSH2)
	{
		"libssh2",
		git_smart_subtransport_ssh_libssh2,
		git_smart_subtransport_ssh_libssh2_set_paths
	},
#endif
#if defined(GIT_SSH_EXEC)
	{
		"exec",
		git_smart_subtransport_ssh_exec,
		git_smart_subtransport_ssh_exec_set_paths
	},
#endif
};

static const git_ssh__backend_t *backend = NULL;

int git_transport_ssh_global_init(void)
{
	return git_ssh__set_backend(DEFAULT_BACKEND_NAME);
}

const char *git_ssh__backend_name(void)
{
	return backend ? backend->name : "";
}

int git_ssh__set_backend(const char *name)
{
	const git_ssh__backend_t *candidate = NULL;
	size_t i;

	/* NULL sets default backend */
	if (!name)
		name = DEFAULT_BACKEND_NAME;

	/* Empty string disables SSH */
	if (!name[0]) {
		backend = NULL;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(backend_table); i++) {
		candidate = &backend_table[i];
		if (!strcmp(name, candidate->name)) {
			backend = candidate;
			return 0;
		}
	}

	git_error_set(GIT_ERROR_INVALID, "library was built without ssh backend '%s'", name);
	return -1;
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
