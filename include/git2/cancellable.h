/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_cancelable_h__
#define INCLUDE_git_cancelable_h__

#include "common.h"
#include "types.h"

/**
 * A cancellable token.
 *
 * You can ask this type whether it's been cancelled and register callbacks for
 * when it gets cancelled.
 */
typedef struct git_cancellable git_cancellable;

/**
 * A cancellable source
 *
 * This owns the cancellable token and can be used to cancel the token.
 */
typedef struct git_cancellable_source git_cancellable_source;

/**
 * Callback for cancellation of a token
 */
typedef int (*git_cancellable_cb)(git_cancellable *cancellable, void *payload);

/**
 * Create a cancellable source
 *
 * Create a new cancellable source. You can pass its token to functions which
 * accept them and cancel them.
 *
 * @param out ponter in which to store the cancellable source
 * @return 0 on success; or an error code
 */
GIT_EXTERN(int) git_cancellable_source_new(git_cancellable_source **out);

/**
 * Free the cancellable source
 *
 * @param cs the cancellable source to free
 */
GIT_EXTERN(void) git_cancellable_source_free(git_cancellable_source *cs);

/**
 * Getter for the cancellation source's token
 *
 * @param cs the cancellable source
 * @return this cancellation source's token
 */
GIT_EXTERN(git_cancellable *) git_cancellable_source_token(git_cancellable_source *cs);

/**
 * Check whether cancellation has been requested
 *
 * @param c the cancellable
 * @return 1 if cancellation has been requested, 0 otherwise
 */
GIT_EXTERN(int) git_cancellable_is_cancelled(git_cancellable *c);

/**
 * Register a cancellable trigger
 *
 * The trigger will be called upon cancellation.
 *
 * @param c the cancellable
 * @param cb the function to call upon cancellation
 * @param payload extra information to pass to the callback
 * @return 0 on success; or an error code
 */
GIT_EXTERN(int) git_cancellable_register(git_cancellable *c, git_cancellable_cb cb, void *payload);

/**
 * Request cancellation
 *
 * If any of the cancellation token's triggers returns an error, no more
 * triggers will be called and the error code will be returned.
 *
 * @param cs the cancellable source to cancel
 * @return 0 on success; or an error code
 */
GIT_EXTERN(int) git_cancellable_source_cancel(git_cancellable_source *cs);

#endif
