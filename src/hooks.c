/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#include <ctype.h>

#include "common.h"
#include "hooks.h"
#include "repository.h"

int git_hooks_discover(git_repository_hooks **hooks_out, git_repository *repo)
{
    int i = 0;
    git_repository_hooks* hooks;
    const char *hooks_to_add[GIT_HOOK_INDEX_MAXIMUM_SUPPORTED];

    assert(repo);
    assert(hooks_out);

    hooks = git__calloc(1, sizeof(git_repository_hooks));
    GITERR_CHECK_ALLOC(hooks);

    git_buf_init(&hooks->path_hooks, 0);
    if (git_buf_joinpath(&hooks->path_hooks, repo->path_repository, GIT_HOOKS_DIRECTORY_NAME) != 0)
    {
        git_repository_hooks_free(hooks);
        return 1;
    }

    if (!git_path_isdir(git_buf_cstr(&hooks->path_hooks)))
    {
        giterr_set(GITERR_OS, "Failed to find directory '%s'", git_buf_cstr(&hooks->path_hooks));
        git_repository_hooks_free(hooks);

        if (errno == ENOENT)
            return GIT_ENOTFOUND;
        return 1;
    }

    /**
    * When a new hook is added, the section below should be updated to include that hook.
    */
    hooks_to_add[GIT_HOOK_INDEX_APPLYPATCH_MSG] = GIT_HOOK_FILENAME_APPLYPATCH_MSG;
    hooks_to_add[GIT_HOOK_INDEX_COMMIT_MSG] = GIT_HOOK_FILENAME_COMMIT_MSG;
    hooks_to_add[GIT_HOOK_INDEX_POST_APPLYPATCH] = GIT_HOOK_FILENAME_POST_APPLYPATCH;
    hooks_to_add[GIT_HOOK_INDEX_POST_CHECKOUT] = GIT_HOOK_FILENAME_POST_CHECKOUT;
    hooks_to_add[GIT_HOOK_INDEX_POST_COMMIT] = GIT_HOOK_FILENAME_POST_COMMIT;
    hooks_to_add[GIT_HOOK_INDEX_POST_MERGE] = GIT_HOOK_FILENAME_POST_MERGE;
    hooks_to_add[GIT_HOOK_INDEX_POST_RECEIVE] = GIT_HOOK_FILENAME_POST_RECEIVE;
    hooks_to_add[GIT_HOOK_INDEX_POST_REWRITE] = GIT_HOOK_FILENAME_POST_REWRITE;
    hooks_to_add[GIT_HOOK_INDEX_POST_UPDATE] = GIT_HOOK_FILENAME_POST_UPDATE;
    hooks_to_add[GIT_HOOK_INDEX_PREPARE_COMMIT_MSG] = GIT_HOOK_FILENAME_PREPARE_COMMIT_MSG;
    hooks_to_add[GIT_HOOK_INDEX_PRE_APPLYPATCH] = GIT_HOOK_FILENAME_PRE_APPLYPATCH;
    hooks_to_add[GIT_HOOK_INDEX_PRE_AUTO_GC] = GIT_HOOK_FILENAME_PRE_AUTO_GC;
    hooks_to_add[GIT_HOOK_INDEX_PRE_COMMIT] = GIT_HOOK_FILENAME_PRE_COMMIT;
    hooks_to_add[GIT_HOOK_INDEX_PRE_PUSH] = GIT_HOOK_FILENAME_PRE_PUSH;
    hooks_to_add[GIT_HOOK_INDEX_PRE_REBASE] = GIT_HOOK_FILENAME_PRE_REBASE;
    hooks_to_add[GIT_HOOK_INDEX_PRE_RECEIVE] = GIT_HOOK_FILENAME_PRE_RECEIVE;
    hooks_to_add[GIT_HOOK_INDEX_UPDATE] = GIT_HOOK_FILENAME_UPDATE;

    for (i = 0; i < ARRAY_SIZE(hooks_to_add); i++)
    {
        if (set_if_hook_exists(hooks, i, hooks_to_add[i]) != 0)
        {
            git_repository_hooks_free(hooks);
            return 1;
        }
    }

    *hooks_out = hooks;

    return 0;
}

void git_repository_hooks_free(git_repository_hooks *hooks)
{
    int i;

    if (hooks == NULL)
        return;

    git_buf_free(&hooks->path_hooks);

    for (i = 0; i < ARRAY_SIZE(hooks->available_hooks); i++)
    {
        git_hook_free(hooks->available_hooks[i]);
        hooks->available_hooks[i] = NULL;
    }

    git__memzero(hooks, sizeof(*hooks));
    git__free(hooks);
}

void git_hook_free(git_hook *hook)
{
    if (hook == NULL)
        return;

    git_buf_free(&hook->file_name);

    git__memzero(hook, sizeof(*hook));
    git__free(hook);
}

int set_if_hook_exists(git_repository_hooks* hooks, int index, const char* file_name)
{
    git_hook *hook = git__calloc(1, sizeof(git_hook));
    GITERR_CHECK_ALLOC(hook);

    hooks->available_hooks[index] = hook;
    
    git_buf_init(&hooks->available_hooks[index]->file_name, 0);
    if (git_buf_sets(&hooks->available_hooks[index]->file_name, file_name) != 0)
    {
        return -1;
    }

    if (git_path_contains_file(&hooks->path_hooks, file_name))
    {
        hooks->available_hooks[index]->exists = TRUE;
    }

    return 0;
}