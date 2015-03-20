/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#ifndef INCLUDE_hooks_h__
#define INCLUDE_hooks_h__

#include "git2/common.h"
#include "git2/hooks.h"

#include "repository.h"

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

void git_hook_free(git_hook *hook);

int set_if_hook_exists(git_repository_hooks* hooks, int index, const char* file_name);

#endif
