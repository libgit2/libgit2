/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#ifndef INCLUDE_git_hooks_h__
#define INCLUDE_git_hooks_h__

#include "types.h"

/**
* @file git2/hooks.h
* @brief Git hooks
* @defgroup git_hooks Git hooks
* @ingroup Git
* @{
*/
GIT_BEGIN_DECL

/**
* The list of indexes for all support hooks.
* 
* Each enum value is used as an index for the array available_hooks
* in the git_hooks struct.
*/
typedef enum {
    GIT_HOOK_INDEX_APPLYPATCH_MSG = 0,
    GIT_HOOK_INDEX_COMMIT_MSG,
    GIT_HOOK_INDEX_POST_APPLYPATCH,
    GIT_HOOK_INDEX_POST_CHECKOUT,
    GIT_HOOK_INDEX_POST_COMMIT,
    GIT_HOOK_INDEX_POST_MERGE,
    GIT_HOOK_INDEX_POST_RECEIVE,
    GIT_HOOK_INDEX_POST_REWRITE,
    GIT_HOOK_INDEX_POST_UPDATE,
    GIT_HOOK_INDEX_PREPARE_COMMIT_MSG,
    GIT_HOOK_INDEX_PRE_APPLYPATCH,
    GIT_HOOK_INDEX_PRE_AUTO_GC,
    GIT_HOOK_INDEX_PRE_COMMIT,
    GIT_HOOK_INDEX_PRE_PUSH,
    GIT_HOOK_INDEX_PRE_REBASE,
    GIT_HOOK_INDEX_PRE_RECEIVE,
    GIT_HOOK_INDEX_UPDATE,

    /**
    * The maximum number of supported hooks.
    *
    * All new hooks should be added above this line.
    */
    GIT_HOOK_INDEX_MAXIMUM_SUPPORTED
} git_hook_indexes;

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
    * The file name of the hook.
    *
    * See `buffer.h` for background on `git_buf` objects.
    */
    git_buf file_name;
} git_hook;

/**
* The hooks for a git repository
*/
typedef struct git_repository_hooks {
    /**
    * The full directory path to the hooks for the git repostiory.
    *
    * See `buffer.h` for background on `git_buf` objects.
    */
    git_buf path_hooks;

    /**
    * An array of all available hooks for the git repository.
    *
    * Each index represents a specific, supported hook for a git
    * repository. Each value in the enum git_hooks is the index for
    * that hook in this array.
    */
    git_hook *available_hooks[GIT_HOOK_INDEX_MAXIMUM_SUPPORTED];
} git_repository_hooks;

/**
* Discover and return the hooks contained in a git repository.
*
* @param out A pointer to the hooks that exist in the git repository.
*
* @param repo A repository object
*
* @return 0 or an error code
*/
GIT_EXTERN(int) git_hooks_discover(
    git_repository_hooks **hooks_out,
    git_repository *repo);

/**
* Deallocate a git repository hooks object.
*
* @param hooks The previously created hooks; cannot be used after free.
*/
GIT_EXTERN(void) git_repository_hooks_free(git_repository_hooks *hooks);

/** @} */
GIT_END_DECL
#endif