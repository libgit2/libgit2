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

static git_hook_callback _hook_callbacks[GIT_HOOK_TYPE_MAXIMUM_SUPPORTED];

int git_hook_get(git_hook **hook_out, git_repository *repo, git_hook_type type)
{
    git_hook* hook = NULL;

    assert(hook_out);
    assert(repo);
    assert(type >= 0 && type <= GIT_HOOK_TYPE_MAXIMUM_SUPPORTED);

    hook = git__calloc(1, sizeof(git_hook));
    GITERR_CHECK_ALLOC(hook);

    git_buf_init(&hook->path, 0);
    if (git_buf_joinpath(&hook->path, repo->path_repository, GIT_HOOKS_DIRECTORY_NAME) != 0)
    {
        git_hook_free(hook);
        return GIT_ERROR;
    }

    const char *file_name = _supported_hooks[type];
    if (git_path_contains_file(&hook->path, file_name))
    {
        hook->exists = TRUE;
    }

    if (git_buf_joinpath(&hook->path, git_buf_cstr(&hook->path), file_name) != 0)
    {
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

void git_hook_register_callback(git_hook_type type, git_hook_callback callback)
{
    assert(type >= 0 && type <= GIT_HOOK_TYPE_MAXIMUM_SUPPORTED);
    assert(callback);

    _hook_callbacks[type] = callback;
}

int git_hook_execute_callback(git_hook_type type, int argv, char *argc)
{
    assert(type >= 0 && type <= GIT_HOOK_TYPE_MAXIMUM_SUPPORTED);
    assert(argv >= 0);
    assert(argc);

    *callback_out = _hook_callbacks[type];
}