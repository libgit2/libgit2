/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_openssl_h__
#define INCLUDE_git_openssl_h__

#include "git2/common.h"

GIT_BEGIN_DECL

/**
 * Initialize the OpenSSL locks
 *
 * OpenSSL requires the application to determine how it performs
 * locking.
 *
 * This is a last-resort convenience function which libgit2 provides for
 * allocating and initializing the locks as well as setting the
 * locking function to use the system's native locking functions.
 *
 * The locking function will be cleared and the memory will be freed
 * when you call git_threads_sutdown().
 *
 * If your programming language has an OpenSSL package/bindings, it
 * likely sets up locking. You should very strongly prefer that over
 * this function.
 *
 * This calls `git_openssl_set_threadsafe()` enabling concurrent
 * OpenSSL operations.
 *
 * @return 0 on success, -1 if there are errors or if libgit2 was not
 * built with OpenSSL and threading support.
 */
GIT_EXTERN(int) git_openssl_set_locking(void);

/**
 * Mark the OpenSSL code as thread-safe
 *
 * By default we take a lock around OpenSSL operations. If you have
 * set up OpenSSL threading in your application, you may call this
 * function to avoid taking these locks, which would enable concurrent
 * work.
 *
 * These locks are only used if the library was built with threading
 * support.
 */
GIT_EXTERN(void) git_openssl_set_threadsafe(void);

GIT_END_DECL
#endif

