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

void test_sparse_init__enables_sparse_checkout(void)
{
	const char *path;
	git_config *config;
	int b;

	git_str content = GIT_STR_INIT;
	git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	path = "sparse/.git/info/sparse-checkout";
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(&opts, g_repo));

	cl_git_pass(git_repository_config(&config, g_repo));
	cl_git_pass(git_config_get_bool(&b, config, "core.sparseCheckout"));
	cl_assert_(b, "sparse checkout should be enabled");
	cl_assert_(git_path_exists(path), path);

	cl_git_pass(git_futils_readbuffer(&content, path));
	cl_assert_equal_s_(git_str_cstr(&content), "", "git_sparse_checkout_init shoiuld init an empty file");

	git_config_free(config);
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