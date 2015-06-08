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
* An individual hook for a git repository
*/
typedef struct git_hook {
    /**
    * Indicates whether the hook exists and is executable. A value of 1 means
    *  the hook exists. A value of 0 means the hook does not exist.
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
* The type of method that is called when a git hook is executed.
*
* @param hook The information about the hook file.
*
* @param repo A repository object.
*
* @param commit_msg_file_path The full directory path to the commit msg.
*
* @return GIT_OK (0) or an error code, error code information dictated by the hook.
*/
typedef int(*git_hook_commit_msg_callback)(git_hook *hook, git_repository *repo, git_buf commit_msg_file_path);

/**
* Retrieve a specific hook contained in a git repository.
*
* @param hook_out An out pointer to the hook.
*
* @param repo A repository object.
*
* @param hook_file_name The file name of the hook
*
* @return GIT_OK (0) or an error code
*/
GIT_EXTERN(int) git_hook_get(git_hook **hook_out, git_repository *repo, const char* hook_file_name);

/**
* Deallocate a git hook object.
*
* @param hooks The previously created hook; cannot be used after free.
*/
GIT_EXTERN(void) git_hook_free(git_hook *hook);

/**
* Registers a callback for the commit-msg hook.
*
* @param callback The callback to register.
*  NULL can be used to de-register a callback.
*/
GIT_EXTERN(void) git_hook_register_commit_msg_callback(git_hook_commit_msg_callback callback);

/** @} */
GIT_END_DECL
#endif
