/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_libssh2_h__
#define INCLUDE_git_libssh2_h__

#include "git2/common.h"

GIT_BEGIN_DECL

/**
 * Mark the libssh2 code as thread-safe
 *
 * By default we take a lock around libssh2 operations, as the
 * thread-safety depends on the caller setting up the threading for
 * the crytographic library it uses. If you have set up its threading,
 * you may call this function to disable the lock, which would enable
 * concurrent work.
 *
 * These locks are only used if the library was built with threading
 * support.
 */
GIT_EXTERN(void) git_libssh2_set_threadsafe(void);

GIT_END_DECL
#endif
