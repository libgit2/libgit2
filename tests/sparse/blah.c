#include "clar_libgit2.h"
#include "futils.h"
#include "sparse.h"
#include "git2/checkout.h"

static git_repository *g_repo = NULL;

void test_sparse_blah__initialize(void)
{
}

void test_sparse_blah__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_sparse_blah__do_it(void)
{
	int error = 0;
//	const char *remote = "/Users/eoinmotherway/Developer/localrepos/_remotes/ConfigAsCodeTest";
	const char *workdir = "/Users/eoinmotherway/Developer/localrepos/lg2_thing";
//	git_futils_mkdir_r(workdir, 0777);

	// Clone the repo, don't checkout
//	git_clone_options clopts;
//	git_clone_options_init(&clopts, 1);

	// clopts.checkout_opts.checkout_strategy = GIT_CHECKOUT_NONE;

	git_repository *repo;
//	git_clone(&repo, remote, workdir, &clopts);
	error = git_repository_init(&repo, workdir, false);

	// Init sparse checkout for the octopus dir
//	char *pattern_strings[] = { ".octopus/ina/" };
//	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
//	git_sparse_checkout_set(&patterns, repo);
//	git_sparse_checkout_disable(repo);
//
//	// Checkout
//	git_checkout_options coopts;
//	git_checkout_options_init(&coopts, 1);
//	git_checkout_head(repo, &coopts);
//
//	git_repository_free(repo);
}
