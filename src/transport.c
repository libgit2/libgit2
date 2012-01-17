/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/types.h"
#include "git2/remote.h"
#include "git2/net.h"
#include "transport.h"

static struct {
	char *prefix;
	git_transport_cb fn;
} transports[] = {
	{"git://", git_transport_git},
	{"http://", git_transport_http},
	{"https://", git_transport_dummy},
	{"file://", git_transport_local},
	{"git+ssh://", git_transport_dummy},
	{"ssh+git://", git_transport_dummy},
	{NULL, 0}
};

#define GIT_TRANSPORT_COUNT (sizeof(transports)/sizeof(transports[0])) - 1

static git_transport_cb transport_find_fn(const char *url)
{
	size_t i = 0;

	 /* TODO: Parse "example.com:project.git" as an SSH URL */

	for (i = 0; i < GIT_TRANSPORT_COUNT; ++i) {
		if (!strncasecmp(url, transports[i].prefix, strlen(transports[i].prefix)))
			return transports[i].fn;
	}

	return NULL;
}

/**************
 * Public API *
 **************/

int git_transport_dummy(git_transport **GIT_UNUSED(transport))
{
	GIT_UNUSED_ARG(transport);
	return git__throw(GIT_ENOTIMPLEMENTED, "This protocol isn't implemented. Sorry");
}

int git_transport_new(git_transport **out, const char *url)
{
	git_transport_cb fn;
	git_transport *transport;
	int error;

	fn = transport_find_fn(url);

	/*
	 * If we haven't found the transport, we assume we mean a
	 * local file.
	 */
	if (fn == NULL)
		fn = &git_transport_local;

	error = fn(&transport);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create new transport");

	transport->url = git__strdup(url);
	if (transport->url == NULL)
		return GIT_ENOMEM;

	*out = transport;

	return GIT_SUCCESS;
}

/* from remote.h */
int git_remote_valid_url(const char *url)
{
	return transport_find_fn(url) != NULL;
}

