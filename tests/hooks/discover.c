#include "clar_libgit2.h"

#include "util.h"
#include "repository.h"
#include "hooks.h"

#define REPO_PATH "hookstestrepo"

static git_repository *_repo = NULL;
static int _expected_hooks[GIT_HOOK_INDEX_MAXIMUM_SUPPORTED];

void test_hooks_discover__initialize(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(_expected_hooks); i++)
    {
        _expected_hooks[i] = 0;
    }

    cl_assert(!git_path_isdir(REPO_PATH));

    cl_git_pass(git_repository_init(&_repo, REPO_PATH, 0));

    cl_assert(git_repository_is_empty(_repo));
}

void test_hooks_discover__cleanup(void)
{
    git_repository_free(_repo);
    _repo = NULL;

    cl_fixture_cleanup(REPO_PATH);
}

static git_buf get_hooks_path()
{
    git_buf hooks_path;
    git_buf_init(&hooks_path, 0);
    cl_assert(git_buf_joinpath(&hooks_path, _repo->path_repository, GIT_HOOKS_DIRECTORY_NAME) == 0);
    return hooks_path;
}

static void add_hook_file(const char* file_name)
{
    git_buf hooks_path;
    git_buf hook_file_path = GIT_BUF_INIT;

    hooks_path = get_hooks_path();
    cl_assert(git_buf_joinpath(&hook_file_path, git_buf_cstr(&hooks_path), file_name) == 0);

    cl_git_mkfile(git_buf_cstr(&hook_file_path), "Test");

    git_buf_free(&hooks_path);
    git_buf_free(&hook_file_path);
}

static void verify_file_name_for_hook(git_repository_hooks *hooks, int index, const char *expected_file_name)
{
    cl_assert_equal_s(git_buf_cstr(&hooks->available_hooks[index]->file_name), expected_file_name);
}

static git_repository_hooks* get_hooks()
{
    git_repository_hooks *hooks = NULL;
    git_buf expected_path;

    cl_git_pass(git_hooks_discover(&hooks, _repo));

    expected_path = get_hooks_path();
    cl_assert(git_buf_cmp(&(hooks->path_hooks), &expected_path) == 0);
    git_buf_free(&expected_path);

    cl_assert_equal_i(ARRAY_SIZE(hooks->available_hooks), GIT_HOOK_INDEX_MAXIMUM_SUPPORTED);

    /**
    * When a new hook is added, the section below should be updated to include that hook.
    */
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_APPLYPATCH_MSG, GIT_HOOK_FILENAME_APPLYPATCH_MSG);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_COMMIT_MSG, GIT_HOOK_FILENAME_COMMIT_MSG);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_POST_APPLYPATCH, GIT_HOOK_FILENAME_POST_APPLYPATCH);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_POST_CHECKOUT, GIT_HOOK_FILENAME_POST_CHECKOUT);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_POST_COMMIT, GIT_HOOK_FILENAME_POST_COMMIT);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_POST_MERGE, GIT_HOOK_FILENAME_POST_MERGE);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_POST_RECEIVE, GIT_HOOK_FILENAME_POST_RECEIVE);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_POST_REWRITE, GIT_HOOK_FILENAME_POST_REWRITE);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_POST_UPDATE, GIT_HOOK_FILENAME_POST_UPDATE);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_PREPARE_COMMIT_MSG, GIT_HOOK_FILENAME_PREPARE_COMMIT_MSG);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_PRE_APPLYPATCH, GIT_HOOK_FILENAME_PRE_APPLYPATCH);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_PRE_AUTO_GC, GIT_HOOK_FILENAME_PRE_AUTO_GC);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_PRE_COMMIT, GIT_HOOK_FILENAME_PRE_COMMIT);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_PRE_PUSH, GIT_HOOK_FILENAME_PRE_PUSH);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_PRE_REBASE, GIT_HOOK_FILENAME_PRE_REBASE);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_PRE_RECEIVE, GIT_HOOK_FILENAME_PRE_RECEIVE);
    verify_file_name_for_hook(hooks, GIT_HOOK_INDEX_UPDATE, GIT_HOOK_FILENAME_UPDATE);

    return hooks;
}

static void check_expected_hooks(git_repository_hooks *hooks)
{
    cl_assert_equal_i(ARRAY_SIZE(hooks->available_hooks), ARRAY_SIZE(_expected_hooks));

    int i;
    for (i = 0; i < ARRAY_SIZE(hooks->available_hooks); i++)
    {
        cl_assert_equal_i(hooks->available_hooks[i]->exists, _expected_hooks[i]);
    }
}

static void cleanup_hooks(git_repository_hooks **hooks)
{
    git_repository_hooks_free((*hooks));
    (*hooks) = NULL;
}

static void get_hooks_and_check_and_cleanup()
{
    git_repository_hooks *hooks = get_hooks();

    check_expected_hooks(hooks);

    cleanup_hooks(&hooks);
}

void test_hooks_discover__verify_no_hooks(void)
{
    get_hooks_and_check_and_cleanup();
}

void test_hooks_discover__verify_some_hooks(void)
{
    add_hook_file(GIT_HOOK_FILENAME_COMMIT_MSG);
    _expected_hooks[GIT_HOOK_INDEX_COMMIT_MSG] = 1;

    add_hook_file(GIT_HOOK_FILENAME_PRE_PUSH);
    _expected_hooks[GIT_HOOK_INDEX_PRE_PUSH] = 1;

    get_hooks_and_check_and_cleanup();
}

void test_hooks_discover__verify_all_hooks(void)
{
    /**
    * Explicitly assume all hooks should be present. This helps to validate
    * that not only all the hooks are present, but that we are not missing
    * any hooks that need to be added in this test.
    */
    int i;
    for (i = 0; i < ARRAY_SIZE(_expected_hooks); i++)
    {
        _expected_hooks[i] = 1;
    }

    add_hook_file(GIT_HOOK_FILENAME_APPLYPATCH_MSG);
    add_hook_file(GIT_HOOK_FILENAME_COMMIT_MSG);
    add_hook_file(GIT_HOOK_FILENAME_POST_APPLYPATCH);
    add_hook_file(GIT_HOOK_FILENAME_POST_CHECKOUT);
    add_hook_file(GIT_HOOK_FILENAME_POST_COMMIT);
    add_hook_file(GIT_HOOK_FILENAME_POST_MERGE);
    add_hook_file(GIT_HOOK_FILENAME_POST_RECEIVE);
    add_hook_file(GIT_HOOK_FILENAME_POST_REWRITE);
    add_hook_file(GIT_HOOK_FILENAME_POST_UPDATE);
    add_hook_file(GIT_HOOK_FILENAME_PREPARE_COMMIT_MSG);
    add_hook_file(GIT_HOOK_FILENAME_PRE_APPLYPATCH);
    add_hook_file(GIT_HOOK_FILENAME_PRE_AUTO_GC);
    add_hook_file(GIT_HOOK_FILENAME_PRE_COMMIT);
    add_hook_file(GIT_HOOK_FILENAME_PRE_PUSH);
    add_hook_file(GIT_HOOK_FILENAME_PRE_REBASE);
    add_hook_file(GIT_HOOK_FILENAME_PRE_RECEIVE);
    add_hook_file(GIT_HOOK_FILENAME_UPDATE);

    get_hooks_and_check_and_cleanup();
}