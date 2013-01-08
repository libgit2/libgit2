/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/types.h"
#include "git2/remote.h"
#include "git2/net.h"
#include "git2/transport.h"
#include "path.h"

typedef struct transport_definition {
	char *prefix;
	unsigned priority;
	git_transport_cb fn;
	void *param;
} transport_definition;

static transport_definition local_transport_definition = { "file://", 1, git_transport_local, NULL };
static transport_definition dummy_transport_definition = { NULL, 1, git_transport_dummy, NULL };

static git_smart_subtransport_definition http_subtransport_definition = { git_smart_subtransport_http, 1 };
static git_smart_subtransport_definition git_subtransport_definition = { git_smart_subtransport_git, 0 };

static transport_definition transports[] = {
	{"git://", 1, git_transport_smart, &git_subtransport_definition},
	{"http://", 1, git_transport_smart, &http_subtransport_definition},
	{"https://", 1, git_transport_smart, &http_subtransport_definition},
	{"file://", 1, git_transport_local, NULL},
	{"git+ssh://", 1, git_transport_dummy, NULL},
	{"ssh+git://", 1, git_transport_dummy, NULL},
	{NULL, 0, 0}
};

#define GIT_TRANSPORT_COUNT (sizeof(transports)/sizeof(transports[0])) - 1

static int transport_find_fn(const char *url, git_transport_cb *callback, void **param)
{
	size_t i = 0;
	unsigned priority = 0;
	transport_definition *definition = NULL, *definition_iter;

	// First, check to see if it's an obvious URL, which a URL scheme
	for (i = 0; i < GIT_TRANSPORT_COUNT; ++i) {
		definition_iter = &transports[i];

		if (strncasecmp(url, definition_iter->prefix, strlen(definition_iter->prefix)))
			continue;

		if (definition_iter->priority > priority)
			definition = definition_iter;
	}

#ifdef GIT_WIN32
	/* On Windows, it might not be possible to discern between absolute local
	 * and ssh paths - first check if this is a valid local path that points
	 * to a directory and if so assume local path, else assume SSH */

	/* Check to see if the path points to a file on the local file system */
	if (!definition && git_path_exists(url) && git_path_isdir(url))
		definition = &local_transport_definition;

	/* It could be a SSH remote path. Check to see if there's a :
	 * SSH is an unsupported transport mechanism in this version of libgit2 */
	if (!definition && strrchr(url, ':'))
		definition = &dummy_transport_definition; 
#else
	/* For other systems, perform the SSH check first, to avoid going to the
	 * filesystem if it is not necessary */

	/* It could be a SSH remote path. Check to see if there's a :
	 * SSH is an unsupported transport mechanism in this version of libgit2 */
	if (!definition && strrchr(url, ':'))
		definition = &dummy_transport_definition;

	/* Check to see if the path points to a file on the local file system */
	if (!definition && git_path_exists(url) && git_path_isdir(url))
		definition = &local_transport_definition;
#endif

	if (!definition)
		return -1;

	*callback = definition->fn;
	*param = definition->param;
	
	return 0;
}

/**************
 * Public API *
 **************/

int git_transport_dummy(git_transport **transport, git_remote *owner, void *param)
{
	GIT_UNUSED(transport);
	GIT_UNUSED(owner);
	GIT_UNUSED(param);
	giterr_set(GITERR_NET, "This transport isn't implemented. Sorry");
	return -1;
}

int git_transport_new(git_transport **out, git_remote *owner, const char *url)
{
	git_transport_cb fn;
	git_transport *transport;
	void *param;
	int error;

	if (transport_find_fn(url, &fn, &param) < 0) {
		giterr_set(GITERR_NET, "Unsupported URL protocol");
		return -1;
	}

	error = fn(&transport, owner, param);
	if (error < 0)
		return error;

	*out = transport;

	return 0;
}

/* from remote.h */
int git_remote_valid_url(const char *url)
{
	git_transport_cb fn;
	void *param;

	return !transport_find_fn(url, &fn, &param);
}

int git_remote_supported_url(const char* url)
{
	git_transport_cb fn;
	void *param;

	if (transport_find_fn(url, &fn, &param) < 0)
		return 0;

	return fn != &git_transport_dummy;
}
