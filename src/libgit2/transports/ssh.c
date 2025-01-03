/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "ssh_exec.h"
#include "ssh_libssh2.h"

#include "transports/smart.h"

/*
 * Default SSH backend. If both backends are compiled in,
 * libssh2 takes precedence over exec for backwards compatibility.
 */
#if defined(GIT_SSH_LIBSSH2)
	#define DEFAULT_BACKEND GIT_SSH_BACKEND_LIBSSH2
#elif defined(GIT_SSH_EXEC)
	#define DEFAULT_BACKEND GIT_SSH_BACKEND_EXEC
#else
	#define DEFAULT_BACKEND GIT_SSH_BACKEND_NONE
#endif

git_ssh_backend_t git_transport__ssh_backend = DEFAULT_BACKEND;

int git_smart_subtransport_ssh(
	git_smart_subtransport **out,
	git_transport *owner,
	void *param)
{
	switch (git_transport__ssh_backend) {
#ifdef GIT_SSH_LIBSSH2
		case GIT_SSH_BACKEND_LIBSSH2:
			return git_smart_subtransport_ssh_libssh2(out, owner, param);
#endif

#ifdef GIT_SSH_EXEC
		case GIT_SSH_BACKEND_EXEC:
			return git_smart_subtransport_ssh_exec(out, owner, param);
#endif

		default:
			git_error_set(GIT_ERROR_INVALID, "cannot create SSH transport; library was built without SSH backend %d", git_transport__ssh_backend);
			return -1;
	}

#if !defined(GIT_SSH_LIBSSH2) && !defined(GIT_SSH_EXEC)
	GIT_UNUSED(out);
	GIT_UNUSED(owner);
	GIT_UNUSED(param);
#endif
}

static int transport_set_paths(git_transport *t, git_strarray *paths)
{
	transport_smart *smart = (transport_smart *)t;

	switch (git_transport__ssh_backend) {
#ifdef GIT_SSH_LIBSSH2
		case GIT_SSH_BACKEND_LIBSSH2:
			return git_smart_subtransport_ssh_libssh2_set_paths(
				(git_smart_subtransport *)smart->wrapped,
				paths->strings[0],
				paths->strings[1]);
#endif

#ifdef GIT_SSH_EXEC
		case GIT_SSH_BACKEND_EXEC:
			return git_smart_subtransport_ssh_exec_set_paths(
				(git_smart_subtransport *)smart->wrapped,
				paths->strings[0],
				paths->strings[1]);
#endif

		default:
			GIT_ASSERT(!"cannot create SSH library; library was built without the requested SSH backend");
			return -1;
	}

#if !defined(GIT_SSH_LIBSSH2) && !defined(GIT_SSH_EXEC)
	GIT_UNUSED(t);
	GIT_UNUSED(smart);
	GIT_UNUSED(paths);
#endif
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

