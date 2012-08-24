/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/types.h"
#include "git2/remote.h"
#include "git2/net.h"
#include "transport.h"
#include "path.h"

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

	// First, check to see if it's an obvious URL, which a URL scheme
	for (i = 0; i < GIT_TRANSPORT_COUNT; ++i) {
		if (!strncasecmp(url, transports[i].prefix, strlen(transports[i].prefix)))
			return transports[i].fn;
	}

	/* still here? Check to see if the path points to a file on the local file system */
	if ((git_path_exists(url) == 0) && git_path_isdir(url))
		return &git_transport_local;

	/* It could be a SSH remote path. Check to see if there's a : */
	if (strrchr(url, ':'))
		return &git_transport_dummy;	/* SSH is an unsupported transport mechanism in this version of libgit2 */

	return NULL;
}

/**************
 * Public API *
 **************/

int git_transport_dummy(git_transport **transport)
{
	GIT_UNUSED(transport);
	giterr_set(GITERR_NET, "This transport isn't implemented. Sorry");
	return -1;
}

int git_transport_new(git_transport **out, const char *url)
{
	git_transport_cb fn;
	git_transport *transport;
	int error;

	fn = transport_find_fn(url);

	if (fn == NULL) {
		giterr_set(GITERR_NET, "Unsupported URL protocol");
		return -1;
	}

	error = fn(&transport);
	if (error < 0)
		return error;

	transport->url = git__strdup(url);
	GITERR_CHECK_ALLOC(transport->url);

	*out = transport;

	return 0;
}

/* from remote.h */
int git_remote_valid_url(const char *url)
{
	return transport_find_fn(url) != NULL;
}

int git_remote_supported_url(const char* url)
{
	git_transport_cb transport_fn = transport_find_fn(url);

	return ((transport_fn != NULL) && (transport_fn != &git_transport_dummy));
}
