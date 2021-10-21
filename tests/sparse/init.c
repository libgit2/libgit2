#include "clar_libgit2.h"
#include "sparse.h"
#include "git2/sparse.h"

static git_repository *g_repo = NULL;

void test_sparse_init__initialize(void)
{
}

void test_sparse_init__cleanup(void)
{
    cl_git_sandbox_cleanup();
}

void test_sparse_init__writes_sparse_checkout_file(void)
{
    const char *path;
    git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

    path = "sparse/.git/info/sparse-checkout";
    g_repo = cl_git_sandbox_init("sparse");

    cl_git_pass(git_sparse_checkout_init(&opts, g_repo));
    cl_assert_(git_path_exists(path), path);
}
