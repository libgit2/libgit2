/*
* Copyright (C) the libgit2 contributors. All rights reserved.
*
* This file is part of libgit2, distributed under the GNU GPL v2 with
* a Linking Exception. For full terms see the included COPYING file.
*/

#include "clar_libgit2.h"

#include "common.h"
#include "path.h"
#include "hooks.h"
#include "repository.h"

#define REPO_PATH "hookstestrepo"

static git_repository *_repo = NULL;

static git_hook_type _callback_expected_hook_type = GIT_HOOK_TYPE_COMMIT_MSG;
static int _callback_expected_argv = 2;
static char *_callback_expected_argc[] = { "Hello", "World" };

static int _callback_called = GIT_HOOK_FALSE;

void test_hooks_callback__initialize(void)
{
    _callback_called = GIT_HOOK_FALSE;

    cl_assert_equal_i(ARRAY_SIZE(_supported_hooks), GIT_HOOK_TYPE_MAXIMUM_SUPPORTED);

    cl_assert(!git_path_isdir(REPO_PATH));

    cl_git_pass(git_repository_init(&_repo, REPO_PATH, 0));

    cl_assert(git_repository_is_empty(_repo));
}

void test_hooks_callback__cleanup(void)
{
    git_repository_free(_repo);
    _repo = NULL;

    cl_fixture_cleanup(REPO_PATH);
}

static int test_callback(git_hook *hook, int argv, char *argc[])
{
    cl_assert_equal_i(hook->type, _callback_expected_hook_type);
    cl_assert_equal_i(argv, _callback_expected_argv);
    cl_assert_equal_p(argc, _callback_expected_argc);

    _callback_called = GIT_HOOK_TRUE;

    return GIT_OK;
}

void test_hooks_callback__verify_callback_register(void)
{
    git_hook_register_callback(_callback_expected_hook_type, test_callback);

    cl_git_pass(git_hook_execute_callback(_callback_expected_hook_type, _repo, _callback_expected_argv, _callback_expected_argc));

    cl_assert_equal_i(_callback_called, GIT_HOOK_TRUE);
}