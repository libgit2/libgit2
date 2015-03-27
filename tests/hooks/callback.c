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
static git_buf _expected_commit_msg_file_path;

static int _callback_called = false;

void test_hooks_callback__initialize(void)
{
    _callback_called = false;

    cl_assert(!git_path_isdir(REPO_PATH));

    cl_git_pass(git_repository_init(&_repo, REPO_PATH, 0));

    cl_assert(git_repository_is_empty(_repo));
}

void test_hooks_callback__cleanup(void)
{
    git_hook_register_commit_msg_callback(NULL);

    git_repository_free(_repo);
    _repo = NULL;

    cl_fixture_cleanup(REPO_PATH);
}

static int verify_callback(git_hook *hook, git_repository *repo, git_buf commit_msg_file_path)
{
    cl_assert_equal_i(hook->exists, false);
    cl_assert(git_buf_contains_nul(&hook->path) != true);
    cl_assert_equal_p(repo, _repo);
    cl_assert_equal_s(git_buf_cstr(&commit_msg_file_path), git_buf_cstr(&_expected_commit_msg_file_path));

    _callback_called = true;

    return GIT_OK;
}

void test_hooks_callback__verify_callback_register(void)
{
    git_buf_init(&_expected_commit_msg_file_path, 0);
    git_buf_puts(&_expected_commit_msg_file_path, "Foobar");

    git_hook_register_commit_msg_callback(verify_callback);

    cl_git_pass(git_hook_execute_commit_msg_callback(_repo, _expected_commit_msg_file_path));

    cl_assert_equal_i(_callback_called, true);

    git_buf_free(&_expected_commit_msg_file_path);
}