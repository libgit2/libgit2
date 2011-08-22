#include "common.h"
#include "git2/types.h"
#include "git2/transport.h"
#include "git2/net.h"
#include "transport.h"

struct {
	char *prefix;
	git_transport_cb fn;
} transports[] = {
	{"git://", git_transport_git},
	{"http://", git_transport_dummy},
	{"https://", git_transport_dummy},
	{"file://", git_transport_local},
	{"git+ssh://", git_transport_dummy},
	{"ssh+git://", git_transport_dummy},
	{NULL, 0}
};

static git_transport_cb transport_new_fn(const char *url)
{
	int i = 0;

	while (1) {
		if (transports[i].prefix == NULL)
			break;

		if (!strncasecmp(url, transports[i].prefix, strlen(transports[i].prefix)))
			return transports[i].fn;

		++i;
	}

	/*
	 * If we still haven't found the transport, we assume we mean a
	 * local file.
	 * TODO: Parse "example.com:project.git" as an SSH URL
	 */
	return git_transport_local;
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

	fn = transport_new_fn(url);

	error = fn(&transport);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create new transport");

	transport->url = git__strdup(url);
	if (transport->url == NULL)
		return GIT_ENOMEM;

	*out = transport;

	return GIT_SUCCESS;
}
