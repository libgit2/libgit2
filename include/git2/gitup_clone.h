#include "clone.h"
#include "common.h"
#include "types.h"
#include "indexer.h"
#include "checkout.h"
#include "remote.h"
#include "transport.h"

/**
 * @file git2/clone.h
 * @brief Git cloning routines
 * @defgroup git_clone Git cloning routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Clone a remote repository.
 *
 * By default this creates its repository and initial remote to match
 * git's defaults. You can use the options in the callback to
 * customize how these are created.
 *
 * @param out pointer that will receive the resulting repository object
 * @param url the remote repository to clone
 * @param local_path local directory to clone to
 * @param options configuration options for the clone.  If NULL, the
 *        function works as though GIT_OPTIONS_INIT were passed.
 * @return 0 on success, any non-zero return value from a callback
 *         function, or a negative value to indicate an error (use
 *         `git_error_last` for a detailed error message)
 */

/// Use `git_clone instead.
GIT_EXTERN(int) gitup_clone_into(
	git_repository **out,
	const char *url,
	const char *local_path,
	const git_clone_options *options
	);

/// Deprecated
/**
 * Clone a remote repository into an existing empty repository using
 * a pre-existing remote.
 *
 * @param repo the repository to clone into
 * @param remote the remote to use for cloning
 * @param fetch_opts the fetch options to use
 * @param checkout_opts the checkout options to use
 * @param branch name of the branch to checkout (NULL means use the
 *        remote's default branch)
 * @return 0 on success, any non-zero return value from a callback
 *         function, or a negative value to indicate an error
 */
GIT_EXTERN(int) gitup_clone_into_old(
	git_repository *repo,
	git_remote *remote,
	const git_fetch_options *fetch_opts,
	const git_checkout_options *checkout_opts,
	const char *branch);

/** @} */
GIT_END_DECL
