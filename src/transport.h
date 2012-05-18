/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transport_h__
#define INCLUDE_transport_h__

#include "git2/net.h"
#include "git2/indexer.h"
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
	int (*ls)(struct git_transport *transport, git_headlist_cb list_cb, void *opaque);
	/**
	 * Push the changes over
	 */
	int (*push)(struct git_transport *transport);
	/**
	 * Negotiate the minimal amount of objects that need to be
	 * retrieved
	 */
	int (*negotiate_fetch)(struct git_transport *transport, git_repository *repo, const git_vector *wants);
	/**
	 * Download the packfile
	 */
	int (*download_pack)(struct git_transport *transport, git_repository *repo, git_off_t *bytes, git_indexer_stats *stats);
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


int git_transport_new(struct git_transport **transport, const char *url);
int git_transport_local(struct git_transport **transport);
int git_transport_git(struct git_transport **transport);
int git_transport_http(struct git_transport **transport);
int git_transport_dummy(struct git_transport **transport);

/**
  Returns true if the passed URL is valid (a URL with a Git supported scheme,
  or pointing to an existing path)
*/
int git_transport_valid_url(const char *url);

typedef struct git_transport git_transport;
typedef int (*git_transport_cb)(git_transport **transport);

#endif
