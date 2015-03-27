/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#include "common.h"
#include "path.h"
#include "hooks.h"
#include "repository.h"

/**
* The callbacks that have been registered
*/
git_hook_commit_msg_callback commit_msg_callback = NULL;

int git_hook_get(git_hook **hook_out, git_repository *repo, const char* hook_file_name)
{
    git_hook *hook = NULL;

    assert(hook_out);
    assert(repo);
    assert(hook_file_name);

    hook = git__calloc(1, sizeof(git_hook));
    GITERR_CHECK_ALLOC(hook);

    git_buf_init(&hook->path, 0);
    if (git_buf_joinpath(&hook->path, repo->path_repository, GIT_HOOKS_DIRECTORY_NAME) != 0) {
        git_hook_free(hook);
        return GIT_ERROR;
    }

    if (git_path_contains_file(&hook->path, hook_file_name)) {
        hook->exists = true;
    }

    if (git_buf_joinpath(&hook->path, git_buf_cstr(&hook->path), hook_file_name) != 0) {
        git_hook_free(hook);
        return GIT_ERROR;
    }

    *hook_out = hook;

    return GIT_OK;
}

void git_hook_free(git_hook *hook)
{
    assert(hook);

    if (hook == NULL)
        return;

    git_buf_free(&hook->path);

    git__memzero(hook, sizeof(*hook));
    git__free(hook);
}

void git_hook_register_commit_msg_callback(git_hook_commit_msg_callback callback)
{
    assert(callback);

    commit_msg_callback = callback;
}

int git_hook_execute_commit_msg_callback(git_repository *repo, git_buf commit_msg_file_path)
{
    int error = GIT_OK;
    git_hook *hook = NULL;

    assert(repo);
    assert(git_buf_contains_nul(&commit_msg_file_path) != true);

    if (commit_msg_callback != NULL)
    {
        if (git_hook_get(&hook, repo, GIT_HOOK_FILENAME_COMMIT_MSG) != 0) {
            git_hook_free(hook);
            return GIT_ERROR;
        }

        error = commit_msg_callback(hook, repo, commit_msg_file_path);

        git_hook_free(hook);
    }

    return error;
}
