/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#ifndef INCLUDE_hooks_h__
#define INCLUDE_hooks_h__

#include "git2/hooks.h"

#define GIT_HOOK_FALSE 0
#define GIT_HOOK_TRUE 1

#define GIT_HOOKS_DIRECTORY_NAME "hooks"

/**
* The file names for each support hook in a git repository.
*/
#define GIT_HOOK_FILENAME_APPLYPATCH_MSG "applypatch-msg"
#define GIT_HOOK_FILENAME_COMMIT_MSG "commit-msg"
#define GIT_HOOK_FILENAME_POST_APPLYPATCH "post-applypatch"
#define GIT_HOOK_FILENAME_POST_CHECKOUT "post-checkout"
#define GIT_HOOK_FILENAME_POST_COMMIT "post-commit"
#define GIT_HOOK_FILENAME_POST_MERGE "post-merge"
#define GIT_HOOK_FILENAME_POST_RECEIVE "post-receive"
#define GIT_HOOK_FILENAME_POST_REWRITE "post-rewrite"
#define GIT_HOOK_FILENAME_POST_UPDATE "post-update"
#define GIT_HOOK_FILENAME_PREPARE_COMMIT_MSG "prepare-commmit-msg"
#define GIT_HOOK_FILENAME_PRE_APPLYPATCH "pre-applypatch"
#define GIT_HOOK_FILENAME_PRE_AUTO_GC "pre-auto-gc"
#define GIT_HOOK_FILENAME_PRE_COMMIT "pre-commit"
#define GIT_HOOK_FILENAME_PRE_PUSH "pre-push"
#define GIT_HOOK_FILENAME_PRE_REBASE "pre-rebase"
#define GIT_HOOK_FILENAME_PRE_RECEIVE "pre-receive"
#define GIT_HOOK_FILENAME_UPDATE "update"

/**
* When a new hook is added, the section below should be updated to include that hook.
*/
static const char* const _supported_hooks[GIT_HOOK_TYPE_MAXIMUM_SUPPORTED] =
{
    GIT_HOOK_FILENAME_APPLYPATCH_MSG,
    GIT_HOOK_FILENAME_COMMIT_MSG,
    GIT_HOOK_FILENAME_POST_APPLYPATCH,
    GIT_HOOK_FILENAME_POST_CHECKOUT,
    GIT_HOOK_FILENAME_POST_COMMIT,
    GIT_HOOK_FILENAME_POST_MERGE,
    GIT_HOOK_FILENAME_POST_RECEIVE,
    GIT_HOOK_FILENAME_POST_REWRITE,
    GIT_HOOK_FILENAME_POST_UPDATE,
    GIT_HOOK_FILENAME_PREPARE_COMMIT_MSG,
    GIT_HOOK_FILENAME_PRE_APPLYPATCH,
    GIT_HOOK_FILENAME_PRE_AUTO_GC,
    GIT_HOOK_FILENAME_PRE_COMMIT,
    GIT_HOOK_FILENAME_PRE_PUSH,
    GIT_HOOK_FILENAME_PRE_REBASE,
    GIT_HOOK_FILENAME_PRE_RECEIVE,
    GIT_HOOK_FILENAME_UPDATE,
};

/**
* Executes the callback for a specific hook.
*
* @param type The type of hook to execute the callback for.
*
* @param argv The number of arguments for the hook, can be 0.
*
* @param argc A pointer to an array containing the arguments, can be null
* if there are no arguments for the hook.
*
* @return GIT_OK (0) if the hook succeeded or there was no callback registered,
* an error code otherwise. Error code information dictated by the hook.
*/
int git_hook_execute_callback(git_hook_type type, git_repository *repo, int argv, char *argc[]);

#endif
