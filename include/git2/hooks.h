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
typedef struct git_repository_hook {
    /**
    * The type of hook.
    */
    git_hook_type type;

    /**
    * Indicates whether the hook exists or not. A value of 1 means
    * the hook exists. A value of 0 means the hook does not exist.
    */
    int exists;

    /**
    * The full directory path to the hook.
    *
    * See `buffer.h` for background on `git_buf` objects.
    */
    git_buf path;
} git_repository_hook;

/**
* The type of method that is called when a git hook is executed.
*
* @param hook The hook that is being executed.
*
* @param repo A repository object.
*
* @param argv The number of arguments for the hook, can be 0.
*
* @param argc A pointer to an array containing the arguments, can be null
* if there are no arguments for the hook.
*
* @return GIT_OK (0) or an error code, error code information dictated by the hook.
*/
typedef int(*git_hook_callback)(git_repository_hook *hook, git_repository *repo, int argv, char *argc[]);

/**
* Retrieve a specific hook contained in a git repository.
*
* @param hook_out An out pointer to the hook.
*
* @param repo A repository object.
*
* @param type The type of hook to get.
*
* @return GIT_OK (0) or an error code
*/
GIT_EXTERN(int) git_repository_hook_get(git_repository_hook **hook_out, git_repository *repo, git_hook_type type);

/**
* Deallocate a git hook object.
*
* @param hooks The previously created hook; cannot be used after free.
*/
GIT_EXTERN(void) git_repository_hook_free(git_repository_hook *hook);

/**
* Registers a callback for a specific hook.
*
* @param type The type of hook to register for.
*
* @param callback The callback to register.
*/
GIT_EXTERN(void) git_repository_hook_register_callback(git_hook_type type, git_hook_callback callback);

/** @} */
GIT_END_DECL
#endif
