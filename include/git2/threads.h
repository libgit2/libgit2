/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_threads_h__
#define INCLUDE_git_threads_h__

#include "common.h"

/**
 * @file git2/threads.h
 * @brief Library level thread functions
 * @defgroup git_thread Threading functions
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Initialize the threading system.
 *
 * If libgit2 has been built with GIT_THREADS on, this function must be
 * called once before any other library functions.
 *
 * If libgit2 has been built without GIT_THREADS
 * support, this function is a no-op.
 *
 * This returns the number of current library users, as defined by
 * the number of callers to `git_threads_init` minus the number of
 * callres to `git_threads_shutdown`.
 *
 * @return the number of current library users (including this one)
 *         or an error code
 */
GIT_EXTERN(int) git_threads_init(void);

/**
 * Shutdown the threading system.
 *
 * If libgit2 has been built with GIT_THREADS on, this function must be
 * called before shutting down the library.
 *
 * The library will be shut down when the last library user (as defined by
 * a previous caller to `git_threads_init`) calls this method.  The number
 * of current library users is returned.
 *
 * If libgit2 has been built without GIT_THREADS support, this function
 * is a no-op.
 *
 * @return the number of current library users or an error code
 */
GIT_EXTERN(int) git_threads_shutdown(void);

/** @} */
GIT_END_DECL
#endif

