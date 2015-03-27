/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#ifndef INCLUDE_hooks_h__
#define INCLUDE_hooks_h__

#include "git2/hooks.h"

#define GIT_HOOKS_DIRECTORY_NAME "hooks"

/**
* The file names for each support hook in a git repository.
*/
#define GIT_HOOK_FILENAME_COMMIT_MSG "commit-msg"
#define GIT_HOOK_FILENAME_POST_CHECKOUT "post-checkout"
#define GIT_HOOK_FILENAME_POST_COMMIT "post-commit"
#define GIT_HOOK_FILENAME_PRE_PUSH "pre-push"
#define GIT_HOOK_FILENAME_PRE_REBASE "pre-rebase"

#define GIT_HOOK_FILENAME_TOTAL 5

/**
* When a new hook is added, the section below should be updated to include that hook.
*/
static const char* const supported_hooks[GIT_HOOK_FILENAME_TOTAL] =
{
    GIT_HOOK_FILENAME_COMMIT_MSG,
    GIT_HOOK_FILENAME_POST_CHECKOUT,
    GIT_HOOK_FILENAME_POST_COMMIT,
    GIT_HOOK_FILENAME_PRE_PUSH,
    GIT_HOOK_FILENAME_PRE_REBASE,
};

/**
* Indicates whether a callback is registered for the commit-msg hook.
*
* @return true if a callback is registered, false otherwise.
*/
int git_hook_is_commit_msg_callback_registered();

/**
* Executes the callback for the commit-msg hook.
*
* @param repo A repository object.
*
* @param commit_msg_file_path The full directory path to the commit msg.
*
* @return GIT_OK (0) if the hook succeeded or there was no callback registered,
* an error code otherwise. Error code information dictated by the hook.
*/
int git_hook_execute_commit_msg_callback(git_repository *repo, git_buf commit_msg_file_path);

#endif
