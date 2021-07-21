/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_custom_tls_h__
#define INCLUDE_sys_custom_tls_h__

#include "git2/common.h"

GIT_BEGIN_DECL

/**
 * Used to retrieve a pointer from a user of the library to pass to a newly
 * created internal libgit2 thread. This should allow users of the library to
 * establish a context that spans an internally threaded operation. This can
 * useful for libraries that leverage callbacks used in an internally threaded
 * routine.
 */
typedef void *GIT_CALLBACK(git_retrieve_tls_for_internal_thread_cb)(void);

/**
 * This callback will be called when a thread is exiting so that a user
 * of the library can clean up their thread local storage.
 */
typedef void GIT_CALLBACK(git_set_tls_on_internal_thread_cb)(void *payload);

/**
 * This callback will be called when a thread is exiting so that a user
 * of the library can clean up their thread local storage.
 */
typedef void GIT_CALLBACK(git_teardown_tls_on_internal_thread_cb)(void);

/**
 * Sets the callbacks for custom thread local storage used by internally
 * created libgit2 threads. This allows users of the library an opportunity
 * to set thread local storage for internal threads based on the creating
 * thread.
 *
 * @param  retrieve_storage_for_internal_thread Used to retrieve a pointer on
 *                                              a thread before spawning child
 *                                              threads. This pointer will be
 *                                              passed to set_storage_on_thread
 *                                              in the newly spawned threads.
 * @param  set_storage_on_thread When a thread is spawned internally in libgit2,
 *                               whatever pointer was retrieved in the calling
 *                               thread by retrieve_storage_for_internal_thread
 *                               will be passed to this callback in the newly
 *                               spawned thread.
 * @param  teardown_storage_on_thread Before an internally spawned thread exits,
 *                                    this method will be called allowing a user
 *                                    of the library an opportunity to clean up
 *                                    any thread local storage they set up on
 *                                    the internal thread.
 * @return 0 on success, or an error code. (use git_error_last for information
 *         about the error)
 */
GIT_EXTERN(int) git_custom_tls_set_callbacks(
	git_retrieve_tls_for_internal_thread_cb retrieve_storage_for_internal_thread,
	git_set_tls_on_internal_thread_cb set_storage_on_thread,
	git_teardown_tls_on_internal_thread_cb teardown_storage_on_thread);

GIT_END_DECL

#endif
