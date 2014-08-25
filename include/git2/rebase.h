/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_rebase_h__
#define INCLUDE_git_rebase_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/rebase.h
 * @brief Git rebase routines
 * @defgroup git_rebase Git merge routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef struct {
	unsigned int version;

	/**
	 * Provide a quiet rebase experience; unused by libgit2 but provided for
	 * interoperability with other clients.
	 */
	int quiet;

	/**
	 * Canonical name of the notes reference used to rewrite notes for
	 * rebased commits when finishing the rebase; if NULL, the contents of
	 * the coniguration option `notes.rewriteRef` is examined, unless the
	 * configuration option `notes.rewrite.rebase` is set to false.  If
	 * `notes.rewriteRef` is NULL, notes will not be rewritten.
	 */
	const char *rewrite_notes_ref;
} git_rebase_options;

/** Type of rebase operation in-progress after calling `git_rebase_next`. */
typedef enum {
	/**
	 * The given commit is to be cherry-picked.  The client should commit
	 * the changes and continue if there are no conflicts.
	 */
	GIT_REBASE_OPERATION_PICK = 0,

	/**
	 * The given commit is to be cherry-picked, but the client should prompt
	 * the user to provide an updated commit message.
	 */
	GIT_REBASE_OPERATION_REWORD,

	/**
	 * The given commit is to be cherry-picked, but the client should stop
	 * to allow the user to edit the changes before committing them.
	 */
	GIT_REBASE_OPERATION_EDIT,

	/**
	 * The given commit is to be squashed into the previous commit.  The
	 * commit message will be merged with the previous message.
	 */
	GIT_REBASE_OPERATION_SQUASH,

	/**
	 * The given commit is to be squashed into the previous commit.  The
	 * commit message from this commit will be discarded.
	 */
	GIT_REBASE_OPERATION_FIXUP,

	/**
	 * No commit will be cherry-picked.  The client should run the given
	 * command and (if successful) continue.
	 */
	GIT_REBASE_OPERATION_EXEC,
} git_rebase_operation_t;

#define GIT_REBASE_OPTIONS_VERSION 1
#define GIT_REBASE_OPTIONS_INIT {GIT_REBASE_OPTIONS_VERSION}

typedef struct {
	/** The type of rebase operation. */
	unsigned int type;

	union {
		/**
		 * The commit ID being cherry-picked.  This will be populated for
		 * all operations except those of type `GIT_REBASE_OPERATION_EXEC`.
		 */
		git_oid id;

		/**
		 * The executable the user has requested be run.  This will only
		 * be populated for operations of type `GIT_REBASE_OPERATION_EXEC`.
		 */
		const char *exec;
	};
} git_rebase_operation;

/**
 * Initializes a `git_rebase_options` with default values. Equivalent to
 * creating an instance with GIT_REBASE_OPTIONS_INIT.
 *
 * @param opts the `git_rebase_options` instance to initialize.
 * @param version the version of the struct; you should pass
 *        `GIT_REBASE_OPTIONS_VERSION` here.
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_rebase_init_options(
	git_rebase_options *opts,
	unsigned int version);

/**
 * Initializes a rebase operation to rebase the changes in `ours`
 * relative to `upstream` onto another branch.  To begin the rebase
 * process, call `git_rebase_next`.  When you have finished with this
 * object, call `git_rebase_free`.
 *
 * @param out Pointer to store the rebase object
 * @param repo The repository to perform the rebase
 * @param branch The terminal commit to rebase
 * @param upstream The commit to begin rebasing from, or NULL to rebase all
 *                 reachable commits
 * @param onto The branch to rebase onto, or NULL to rebase onto the given
 *             upstream
 * @param signature The signature of the rebaser
 * @param opts Options to specify how rebase is performed
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_rebase_init(
	git_rebase **out,
	git_repository *repo,
	const git_merge_head *branch,
	const git_merge_head *upstream,
	const git_merge_head *onto,
	const git_signature *signature,
	const git_rebase_options *opts);

/**
 * Opens an existing rebase that was previously started by either an
 * invocation of `git_rebase_init` or by another client.
 *
 * @param out Pointer to store the rebase object
 * @param reop The repository that has a rebase in-progress
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_rebase_open(git_rebase **out, git_repository *repo);

/**
 * Performs the next rebase operation and returns the information about it.
 * If the operation is one that applies a patch (which is any operation except
 * GIT_REBASE_OPERATION_EXEC) then the patch will be applied and the index and
 * working directory will be updated with the changes.  If there are conflicts,
 * you will need to address those before committing the changes.
 *
 * @param out The rebase operation that is to be performed next
 * @param repo The rebase in progress
 * @param checkout_opts Options to specify how the patch should be checked out
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_rebase_next(
	git_rebase_operation *operation,
	git_rebase *rebase,
	git_checkout_options *checkout_opts);

/**
 * Commits the current patch.  You must have resolved any conflicts that
 * were introduced during the patch application from the `git_rebase_next`
 * invocation.
 *
 * @param id Pointer in which to store the OID of the newly created commit
 * @param repo The rebase that is in-progress
 * @param author The author of the updated commit, or NULL to keep the
 *        author from the original commit
 * @param committer The committer of the rebase
 * @param message_encoding The encoding for the message in the commit,
 *        represented with a standard encoding name.  If message is NULL,
 *        this should also be NULL, and the encoding from the original
 *        commit will be maintained.  If message is specified, this may be
 *        NULL to indicate that "UTF-8" is to be used.
 * @param message The message for this commit, or NULL to use the message
 *        from the original commit.
 * @return Zero on success, GIT_EUNMERGED if there are unmerged changes in
 *        the index, GIT_EAPPLIED if the current commit has already
 *        been applied to the upstream and there is nothing to commit,
 *        -1 on failure.
 */
GIT_EXTERN(int) git_rebase_commit(
	git_oid *id,
	git_rebase *rebase,
	const git_signature *author,
	const git_signature *committer,
	const char *message_encoding,
	const char *message);

/**
 * Aborts a rebase that is currently in progress, resetting the repository
 * and working directory to their state before rebase began.
 *
 * @param rebase The rebase that is in-progress
 * @param signature The identity that is aborting the rebase
 * @return Zero on success; GIT_ENOTFOUND if a rebase is not in progress,
 *         -1 on other errors.
 */
GIT_EXTERN(int) git_rebase_abort(
	git_rebase *rebase,
	const git_signature *signature);

/**
 * Finishes a rebase that is currently in progress once all patches have
 * been applied.
 *
 * @param rebase The rebase that is in-progress
 * @param signature The identity that is finishing the rebase
 * @param opts Options to specify how rebase is finished
 * @param Zero on success; -1 on error
 */
GIT_EXTERN(int) git_rebase_finish(
	git_rebase *rebase,
	const git_signature *signature,
	const git_rebase_options *opts);

/**
 * Frees the `git_rebase` object.
 *
 * @param rebase The rebase that is in-progress
 */
GIT_EXTERN(void) git_rebase_free(git_rebase *rebase);

/** @} */
GIT_END_DECL
#endif
