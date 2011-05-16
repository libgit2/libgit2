#include "common.h"
#include "git2/types.h"
#include "git2/transport.h"
#include "git2/net.h"
#include "git2/repository.h"
#include "transport.h"

/*
 * Try to open the url as a git directory. The direction doesn't
 * matter in this case because we're calulating the heads ourselves.
 */
static int local_connect(git_transport *transport, git_net_direction GIT_UNUSED(dir))
{
	git_repository *repo;
	int error;

	error = git_repository_open(&repo, transport->url);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Can't open remote");

	transport->private = repo;

	return GIT_SUCCESS;
}

static int local_ls(git_transport *transport, git_headarray *array)
{
	return GIT_SUCCESS;
}

/**************
 * Public API *
 **************/

int git_transport_local(git_transport *transport)
{
	transport->connect = local_connect;
	transport->ls = local_ls;

	return GIT_SUCCESS;
}
