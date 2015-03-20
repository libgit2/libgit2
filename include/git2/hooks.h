/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#ifndef INCLUDE_git_hooks_h__
#define INCLUDE_git_hooks_h__

#include "types.h"
#include "buffer.h"

/**
* @file git2/hooks.h
* @brief Git hooks
* @defgroup git_hooks Git hooks
* @ingroup Git
* @{
*/
GIT_BEGIN_DECL

/**
* The list of all support hooks.
*/
typedef enum {
    GIT_HOOK_TYPE_APPLYPATCH_MSG = 0,
    GIT_HOOK_TYPE_COMMIT_MSG,
    GIT_HOOK_TYPE_POST_APPLYPATCH,
    GIT_HOOK_TYPE_POST_CHECKOUT,
    GIT_HOOK_TYPE_POST_COMMIT,
    GIT_HOOK_TYPE_POST_MERGE,
    GIT_HOOK_TYPE_POST_RECEIVE,
    GIT_HOOK_TYPE_POST_REWRITE,
    GIT_HOOK_TYPE_POST_UPDATE,
    GIT_HOOK_TYPE_PREPARE_COMMIT_MSG,
    GIT_HOOK_TYPE_PRE_APPLYPATCH,
    GIT_HOOK_TYPE_PRE_AUTO_GC,
    GIT_HOOK_TYPE_PRE_COMMIT,
    GIT_HOOK_TYPE_PRE_PUSH,
    GIT_HOOK_TYPE_PRE_REBASE,
    GIT_HOOK_TYPE_PRE_RECEIVE,
    GIT_HOOK_TYPE_UPDATE,

    /**
    * The maximum number of supported hooks.
    *
    * All new hooks should be added above this line.
    */
    GIT_HOOK_TYPE_MAXIMUM_SUPPORTED
} git_hook_type;

/**
* An individual hook for a git repository
*/
typedef struct git_hook {
    /**
    * Indicates whether the hook exists or not. A value of TRUE means
    * the hook exists. A value of FALSE means the hook does not exist.
    */
    int exists;

    /**
    * The full directory path to the hook.
    *
    * See `buffer.h` for background on `git_buf` objects.
    */
    git_buf path;
} git_hook;

/**
* Retrieve a specific hook contained in a git repository.
*
* @param hook_out An out pointer to the hook.
*
* @param repo A repository object.
*
* @param type The type of hook to get.
*
* @return 0 or an error code
*/
GIT_EXTERN(int) git_hooks_get(
    git_hook **hook_out,
    git_repository *repo,
    git_hook_type type);

/**
* Deallocate a git hook object.
*
* @param hooks The previously created hook; cannot be used after free.
*/
GIT_EXTERN(void) git_hook_free(git_hook *hook);

/** @} */
GIT_END_DECL
#endif
