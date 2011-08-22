#ifndef INCLUDE_transport_h__
#define INCLUDE_transport_h__

#include "git2/transport.h"
#include "git2/net.h"
#include "vector.h"

#define GIT_CAP_OFS_DELTA "ofs-delta"

typedef struct git_transport_caps {
	int common:1,
	    ofs_delta:1;
} git_transport_caps;

/*
 * A day in the life of a network operation
 * ========================================
 *
 * The library gets told to ls-remote/push/fetch on/to/from some
 * remote. We look at the URL of the remote and fill the function
 * table with whatever is appropriate (the remote may be git over git,
 * ssh or http(s). It may even be an hg or svn repository, the library
 * at this level doesn't care, it just calls the helpers.
 *
 * The first call is to ->connect() which connects to the remote,
 * making use of the direction if necessary. This function must also
 * store the remote heads and any other information it needs.
 *
 * The next useful step is to call ->ls() to get the list of
 * references available to the remote. These references may have been
 * collected on connect, or we may build them now. For ls-remote,
 * nothing else is needed other than closing the connection.
 * Otherwise, the higher leves decide which objects we want to
 * have. ->send_have() is used to tell the other end what we have. If
 * we do need to download a pack, ->download_pack() is called.
 *
 * When we're done, we call ->close() to close the
 * connection. ->free() takes care of freeing all the resources.
 */

struct git_transport {
	/**
	 * Where the repo lives
	 */
	char *url;
	/**
	 * Whether we want to push or fetch
	 */
	int direction : 1, /* 0 fetch, 1 push */
		connected : 1;
	/**
	 * Connect and store the remote heads
	 */
	int (*connect)(struct git_transport *transport, int dir);
	/**
	 * Give a list of references, useful for ls-remote
	 */
	int (*ls)(struct git_transport *transport, git_headarray *headarray);
	/**
	 * Push the changes over
	 */
	int (*push)(struct git_transport *transport);
	/**
	 * Send the list of 'want' refs
	 */
	int (*send_wants)(struct git_transport *transport, git_headarray *list);
	/**
	 * Send the list of 'have' refs
	 */
	int (*send_have)(struct git_transport *transport, git_oid *oid);
	/**
	 * Send a 'done' message
	 */
	int (*send_done)(struct git_transport *transport);
	/**
	 * Negotiate the minimal amount of objects that need to be
	 * retrieved
	 */
	int (*negotiate_fetch)(struct git_transport *transport, git_repository *repo, git_headarray *list);
	/**
	 * Send a flush
	 */
	int (*send_flush)(struct git_transport *transport);
	/**
	 * Download the packfile
	 */
	int (*download_pack)(char **out, struct git_transport *transport, git_repository *repo);
	/**
	 * Fetch the changes
	 */
	int (*fetch)(struct git_transport *transport);
	/**
	 * Close the connection
	 */
	int (*close)(struct git_transport *transport);
	/**
	 * Free the associated resources
	 */
	void (*free)(struct git_transport *transport);
};

int git_transport_local(struct git_transport **transport);
int git_transport_git(struct git_transport **transport);
int git_transport_dummy(struct git_transport **transport);

#endif
