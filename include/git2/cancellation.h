/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_cancellation_h__
#define INCLUDE_git_cancellation_h__

#include "common.h"
#include "types.h"

/**
 * Synchronization for canceling operatons
 *
 * You can ask this type whether it's been cancelled, cancel it and register
 * callbacks for when it gets cancelled.
 */
typedef struct git_cancellation git_cancellation;

/**
 * Callback for a cancellation triggering
 */
typedef int (*git_cancellation_cb)(git_cancellation *cancellation, void *payload);

/**
 * Create a cancellation token
 *
 * Create a new cancellable source. You can pass its token to functions which
 * accept them and cancel them.
 *
 * @param out ponter in which to store the cancellation
 * @return 0 on success; or an error code
 */
GIT_EXTERN(int) git_cancellation_new(git_cancellation **out);

/**
 * Free the cancellation
 *
 * @param c the cancellation to free
 */
GIT_EXTERN(void) git_cancellation_free(git_cancellation *c);

/**
 * Check whether cancellation has been requested
 *
 * @param c the cancellation
 * @return 1 if cancellation has been requested, 0 otherwise
 */
GIT_EXTERN(int) git_cancellation_requested(git_cancellation *c);

/**
 * Register a cancellable trigger
 *
 * The trigger will be called upon cancellation.
 *
 * @param c the cancellation
 * @param cb the function to call upon cancellation
 * @param payload extra information to pass to the callback
 * @return 0 on success; or an error code
 */
GIT_EXTERN(int) git_cancellation_register(git_cancellation *c, git_cancellation_cb cb, void *payload);

/**
 * Request cancellation
 *
 * If any of the cancellation token's triggers returns an error, no more
 * triggers will be called and the error code will be returned.
 *
 * @param c the cancellable source to cancel
 * @return 0 on success; or an error code
 */
GIT_EXTERN(int) git_cancellation_request(git_cancellation *c);

#endif
