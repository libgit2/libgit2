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

static git_hook_callback registered_callbacks[GIT_HOOK_TYPE_MAXIMUM_SUPPORTED];

int git_repository_hook_get(git_repository_hook **hook_out, git_repository *repo, git_hook_type type)
{
    git_repository_hook *hook = NULL;
    const char *file_name;

    assert(hook_out);
    assert(repo);
    assert(type >= 0 && type <= GIT_HOOK_TYPE_MAXIMUM_SUPPORTED);

    hook = git__calloc(1, sizeof(git_repository_hook));
    GITERR_CHECK_ALLOC(hook);

    hook->type = type;

    git_buf_init(&hook->path, 0);
    if (git_buf_joinpath(&hook->path, repo->path_repository, GIT_HOOKS_DIRECTORY_NAME) != 0) {
        git_repository_hook_free(hook);
        return GIT_ERROR;
    }

    file_name = supported_hooks[type];
    if (git_path_contains_file(&hook->path, file_name)) {
        hook->exists = GIT_HOOK_TRUE;
    }

    if (git_buf_joinpath(&hook->path, git_buf_cstr(&hook->path), file_name) != 0) {
        git_repository_hook_free(hook);
        return GIT_ERROR;
    }

    *hook_out = hook;

    return GIT_OK;
}

void git_repository_hook_free(git_repository_hook *hook)
{
    assert(hook);

    if (hook == NULL)
        return;

    git_buf_free(&hook->path);

    git__memzero(hook, sizeof(*hook));
    git__free(hook);
}

void git_repository_hook_register_callback(git_hook_type type, git_hook_callback callback)
{
    assert(type >= 0 && type <= GIT_HOOK_TYPE_MAXIMUM_SUPPORTED);
    assert(callback);

    registered_callbacks[type] = callback;
}

int git_repository_hook_execute_callback(git_hook_type type, git_repository *repo, int argv, char *argc[])
{
    int error = GIT_OK;
    git_repository_hook *hook = NULL;

    assert(type >= 0 && type <= GIT_HOOK_TYPE_MAXIMUM_SUPPORTED);
    assert(argv >= 0);
    assert(argc);

    if (registered_callbacks[type] != NULL) {
        if (git_repository_hook_get(&hook, repo, type) != GIT_OK) {
            return GIT_ERROR;
        }

        error = registered_callbacks[type](hook, repo, argv, argc);

        git_repository_hook_free(hook);
    }

    return error;
}
