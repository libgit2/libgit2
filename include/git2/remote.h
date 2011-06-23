#ifndef INCLUDE_git_remote_h__
#define INCLUDE_git_remote_h__

#include "git2/common.h"
#include "git2/repository.h"
#include "git2/refspec.h"

/*
 * TODO: This functions still need to be implemented:
 * - _listcb/_foreach
 * - _add
 * - _rename
 * - _del (needs support from config)
 */

/**
 * Get the information for a particular remote
 *
 * @param out pointer to the new remote object
 * @param cfg the repository's configuration
 * @param name the remote's name
 * @return 0 on success; error value otherwise
 */
GIT_EXTERN(int) git_remote_get(struct git_remote **out, struct git_config *cfg, const char *name);

/**
 * Get the remote's name
 *
 * @param remote the remote
 * @return a pointer to the name
 */
GIT_EXTERN(const char *) git_remote_name(struct git_remote *remote);

/**
 * Get the remote's url
 *
 * @param remote the remote
 * @return a pointer to the url
 */
GIT_EXTERN(const char *) git_remote_url(struct git_remote *remote);

/**
 * Get the fetch refspec
 *
 * @param remote the remote
 * @return a pointer to the fetch refspec or NULL if it doesn't exist
 */
GIT_EXTERN(const git_refspec *) git_remote_fetchspec(struct git_remote *remote);

/**
 * Get the push refspec
 *
 * @param remote the remote
 * @return a pointer to the push refspec or NULL if it doesn't exist
 */

GIT_EXTERN(const git_refspec *) git_remote_fetchspec(struct git_remote *remote);

/**
 * Open a connection to a remote
 *
 * The transport is selected based on the URL
 *
 * @param remote the remote to connect to
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_remote_connect(struct git_remote *remote, git_net_direction dir);

/**
 * Get a list of refs at the remote
 *
 * The remote (or more exactly its transport) must be connected.
 *
 * @param refs where to store the refs
 * @param remote the remote
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_remote_ls(git_remote *remote, git_headarray *refs);

/**
 * Free the memory associated with a remote
 *
 * @param remote the remote to free
 */
GIT_EXTERN(void) git_remote_free(struct git_remote *remote);

#endif
