/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_checkout_h__
#define INCLUDE_git_checkout_h__

#include "common.h"
#include "types.h"
#include "diff.h"

/**
 * @file git2/checkout.h
 * @brief Git checkout routines
 * @defgroup git_checkout Git checkout routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Checkout behavior flags
 *
 * In libgit2, the function of checkout is to update the working directory
 * to match a target tree.  It does not move the HEAD commit - you do that
 * separately.  To safely perform the update, checkout relies on a baseline
 * tree (generally the current HEAD) as a reference for the unmodified
 * content expected in the working directory.
 *
 * Checkout examines the differences between the target tree, the baseline
 * tree and the working directory, and groups files into five categories:
 *
 * 1. UNMODIFIED - Files that match in all places.
 * 2. SAFE - Files where the working directory and the baseline content
 *    match that can be safely updated to the target.
 * 3. DIRTY/MISSING - Files where the working directory differs from the
 *    baseline but there is no conflicting change with the target.  One
 *    example is a file that doesn't exist in the working directory - no
 *    data would be lost as a result of writing this file.  Which action
 *    will be taken with these files depends on the options you use.
 * 4. CONFLICTS - Files where changes in the working directory conflict
 *    with changes to be applied by the target.  If conflicts are found,
 *    they prevent any other modifications from being made (although there
 *    are options to override that and force the update, of course).
 * 5. UNTRACKED/IGNORED - Files in the working directory that are untracked
 *    or ignored (i.e. only in the working directory, not the other places).
 *
 *
 * You control the actions checkout takes with one of four base strategies:
 *
 * - `GIT_CHECKOUT_NONE` is the default and applies no changes. It is a dry
 *   run that you can use to find conflicts, etc. if you wish.
 *
 * - `GIT_CHECKOUT_SAFE` is like `git checkout` and only applies changes
 *   between the baseline and target trees to files in category 2.
 *
 * - `GIT_CHECKOUT_SAFE_CREATE` also creates files that are missing from the
 *   working directory (category 3), even if there is no change between the
 *   baseline and target trees for those files.  See notes below on
 *   emulating `git checkout-index` for some of the subtleties of this.
 *
 * - `GIT_CHECKOUT_FORCE` is like `git checkout -f` and will update the
 *   working directory to match the target content regardless of conflicts,
 *   overwriting dirty and conflicting files.
 *
 *
 * There are some additional flags to modified the behavior of checkout:
 *
 * - GIT_CHECKOUT_ALLOW_CONFLICTS can be added to apply safe file updates
 *   even if there are conflicts.  Normally, the entire checkout will be
 *   cancelled if any files are in category 4.  With this flag, conflicts
 *   will be skipped (though the notification callback will still be invoked
 *   on the conflicting files if requested).
 *
 * - GIT_CHECKOUT_REMOVE_UNTRACKED means that files in the working directory
 *   that are untracked (but not ignored) should be deleted.  The are not
 *   considered conflicts and would normally be ignored by checkout.
 *
 * - GIT_CHECKOUT_REMOVE_IGNORED means to remove ignored files from the
 *   working directory as well.  Obviously, these would normally be ignored.
 *
 * - GIT_CHECKOUT_UPDATE_ONLY means to only update the content of files that
 *   already exist.  Files will not be created nor deleted.  This does not
 *   make adds and deletes into conflicts - it just skips applying those
 *   changes.  This will also skip updates to typechanged files (since that
 *   would involve deleting the old and creating the new).
 *
 * - Unmerged entries in the index are also considered conflicts.  The
 *   GIT_CHECKOUT_SKIP_UNMERGED flag causes us to skip files with unmerged
 *   index entries.  You can also use GIT_CHECKOUT_USE_OURS and
 *   GIT_CHECKOUT_USE_THEIRS to proceeed with the checkout using either the
 *   stage 2 ("ours") or stage 3 ("theirs") version of files in the index.
 *
 *
 * To emulate `git checkout`, use `GIT_CHECKOUT_SAFE` with a checkout
 * notification callback (see below) that displays information about dirty
 * files (i.e. files that don't need an update but that no longer match the
 * baseline content).  The default behavior will cancel on conflicts.
 *
 * To emulate `git checkout-index`, use `GIT_CHECKOUT_SAFE_CREATE` with a
 * notification callback that cancels the operation if a dirty-but-existing
 * file is found in the working directory.  This core git command isn't
 * quite "force" but is sensitive about some types of changes.
 *
 * To emulate `git checkout -f`, you use `GIT_CHECKOUT_FORCE`.
 *
 *
 * Checkout is "semi-atomic" as in it will go through the work to be done
 * before making any changes and if may decide to abort if there are
 * conflicts, or you can use the notification callback to explicitly abort
 * the action before any updates are made.  Despite this, if a second
 * process is modifying the filesystem while checkout is running, it can't
 * guarantee that the choices is makes while initially examining the
 * filesystem are still going to be correct as it applies them.
 */
typedef enum {
	GIT_CHECKOUT_NONE = 0, /** default is a dry run, no actual updates */

	/** Allow safe updates that cannot overwrite uncommited data */
	GIT_CHECKOUT_SAFE = (1u << 0),

	/** Allow safe updates plus creation of missing files */
	GIT_CHECKOUT_SAFE_CREATE = (1u << 1),

	/** Allow all updates to force working directory to look like index */
	GIT_CHECKOUT_FORCE = (1u << 2),


	/** Allow checkout to make safe updates even if conflicts are found */
	GIT_CHECKOUT_ALLOW_CONFLICTS = (1u << 4),

	/** Remove untracked files not in index (that are not ignored) */
	GIT_CHECKOUT_REMOVE_UNTRACKED = (1u << 5),

	/** Remove ignored files not in index */
	GIT_CHECKOUT_REMOVE_IGNORED = (1u << 6),

	/** Only update existing files, don't create new ones */
	GIT_CHECKOUT_UPDATE_ONLY = (1u << 7),

	/** Normally checkout updates index entries as it goes; this stops that */
	GIT_CHECKOUT_DONT_UPDATE_INDEX = (1u << 8),

	/** Don't refresh index/config/etc before doing checkout */
	GIT_CHECKOUT_NO_REFRESH = (1u << 9),

	/**
	 * THE FOLLOWING OPTIONS ARE NOT YET IMPLEMENTED
	 */

	/** Allow checkout to skip unmerged files (NOT IMPLEMENTED) */
	GIT_CHECKOUT_SKIP_UNMERGED = (1u << 10),
	/** For unmerged files, checkout stage 2 from index (NOT IMPLEMENTED) */
	GIT_CHECKOUT_USE_OURS = (1u << 11),
	/** For unmerged files, checkout stage 3 from index (NOT IMPLEMENTED) */
	GIT_CHECKOUT_USE_THEIRS = (1u << 12),

	/** Recursively checkout submodules with same options (NOT IMPLEMENTED) */
	GIT_CHECKOUT_UPDATE_SUBMODULES = (1u << 16),
	/** Recursively checkout submodules if HEAD moved in super repo (NOT IMPLEMENTED) */
	GIT_CHECKOUT_UPDATE_SUBMODULES_IF_CHANGED = (1u << 17),

} git_checkout_strategy_t;

/**
 * Checkout notification flags
 *
 * When running a checkout, you can set a notification callback (`notify_cb`)
 * to be invoked for some or all files to be checked out.  Which files
 * receive a callback depend on the `notify_flags` value which is a
 * combination of these flags.
 *
 * - GIT_CHECKOUT_NOTIFY_CONFLICT means that conflicting files that would
 *   prevent the checkout from occurring will receive callbacks.  If you
 *   used GIT_CHECKOUT_ALLOW_CONFLICTS, the callbacks are still done, but
 *   the checkout will not be blocked.  The callback `status_flags` will
 *   have both index and work tree change bits set (see `git_status_t`).
 *
 * - GIT_CHECKOUT_NOTIFY_DIRTY means to notify about "dirty" files, i.e.
 *   those that do not need to be updated but no longer match the baseline
 *   content.  Core git displays these files when checkout runs, but does
 *   not stop the checkout.  For these,  `status_flags` will have only work
 *   tree bits set (i.e. GIT_STATUS_WT_MODIFIED, etc).
 *
 * - GIT_CHECKOUT_NOTIFY_UPDATED sends notification for any file changed by
 *   the checkout.  Callback `status_flags` will have only index bits set.
 *
 * - GIT_CHECKOUT_NOTIFY_UNTRACKED notifies for all untracked files that
 *   are not ignored.  Passing GIT_CHECKOUT_REMOVE_UNTRACKED would remove
 *   these files.  The `status_flags` will be GIT_STATUS_WT_NEW.
 *
 * - GIT_CHECKOUT_NOTIFY_IGNORED notifies for the ignored files.  Passing
 *   GIT_CHECKOUT_REMOVE_IGNORED will remove these.  The `status_flags`
 *   will be to GIT_STATUS_IGNORED.
 *
 * If you return a non-zero value from the notify callback, the checkout
 * will be canceled.  Notification callbacks are made prior to making any
 * modifications, so returning non-zero will cancel the entire checkout.
 * If you are do not use GIT_CHECKOUT_ALLOW_CONFLICTS and there are
 * conflicts, you don't need to explicitly cancel from the callback.
 * Checkout itself will abort after all files are processed.
 *
 * To emulate core git checkout output, use GIT_CHECKOUT_NOTIFY_CONFLICTS
 * and GIT_CHECKOUT_NOTIFY_DIRTY.  Conflicts will have `status_flags` with
 * changes in both the index and work tree (see the `git_status_t` values).
 * Dirty files will only have work tree flags set.
 */
typedef enum {
	GIT_CHECKOUT_NOTIFY_NONE      = 0,
	GIT_CHECKOUT_NOTIFY_CONFLICT  = (1u << 0),
	GIT_CHECKOUT_NOTIFY_DIRTY     = (1u << 1),
	GIT_CHECKOUT_NOTIFY_UPDATED   = (1u << 2),
	GIT_CHECKOUT_NOTIFY_UNTRACKED = (1u << 3),
	GIT_CHECKOUT_NOTIFY_IGNORED   = (1u << 4),
} git_checkout_notify_t;

/**
 * Checkout options structure
 *
 * Use zeros to indicate default settings.
 *
 * This should be initialized with the `GIT_CHECKOUT_OPTS_INIT` macro to
 * correctly set the `version` field.
 *
 *		git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
 */
typedef struct git_checkout_opts {
	unsigned int version;

	unsigned int checkout_strategy; /** default will be a dry run */

	int disable_filters;    /** don't apply filters like CRLF conversion */
	unsigned int dir_mode;  /** default is 0755 */
	unsigned int file_mode; /** default is 0644 or 0755 as dictated by blob */
	int file_open_flags;    /** default is O_CREAT | O_TRUNC | O_WRONLY */

	unsigned int notify_flags; /** see `git_checkout_notify_t` above */
	int (*notify_cb)(
		git_checkout_notify_t why,
		const char *path,
		const git_diff_file *baseline,
		const git_diff_file *target,
		const git_diff_file *workdir,
		void *payload);
	void *notify_payload;

	/* Optional callback to notify the consumer of checkout progress. */
	void (*progress_cb)(
		const char *path,
		size_t completed_steps,
		size_t total_steps,
		void *payload);
	void *progress_payload;

	/** When not zeroed out, array of fnmatch patterns specifying which
	 *  paths should be taken into account, otherwise all files.
	 */
	git_strarray paths;

	git_tree *baseline; /** expected content of workdir, defaults to HEAD */
} git_checkout_opts;

#define GIT_CHECKOUT_OPTS_VERSION 1
#define GIT_CHECKOUT_OPTS_INIT {GIT_CHECKOUT_OPTS_VERSION}

/**
 * Updates files in the index and the working tree to match the content of
 * the commit pointed at by HEAD.
 *
 * @param repo repository to check out (must be non-bare)
 * @param opts specifies checkout options (may be NULL)
 * @return 0 on success, GIT_EORPHANEDHEAD when HEAD points to a non existing
 * branch, GIT_ERROR otherwise (use giterr_last for information
 * about the error)
 */
GIT_EXTERN(int) git_checkout_head(
	git_repository *repo,
	git_checkout_opts *opts);

/**
 * Updates files in the working tree to match the content of the index.
 *
 * @param repo repository into which to check out (must be non-bare)
 * @param index index to be checked out (or NULL to use repository index)
 * @param opts specifies checkout options (may be NULL)
 * @return 0 on success, GIT_ERROR otherwise (use giterr_last for information
 * about the error)
 */
GIT_EXTERN(int) git_checkout_index(
	git_repository *repo,
	git_index *index,
	git_checkout_opts *opts);

/**
 * Updates files in the index and working tree to match the content of the
 * tree pointed at by the treeish.
 *
 * @param repo repository to check out (must be non-bare)
 * @param treeish a commit, tag or tree which content will be used to update
 * the working directory
 * @param opts specifies checkout options (may be NULL)
 * @return 0 on success, GIT_ERROR otherwise (use giterr_last for information
 * about the error)
 */
GIT_EXTERN(int) git_checkout_tree(
	git_repository *repo,
	const git_object *treeish,
	git_checkout_opts *opts);

/** @} */
GIT_END_DECL
#endif
