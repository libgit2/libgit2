#include "clar_libgit2.h"
#include "checkout_helpers.h"
#include "../filter/crlf.h"

#include "git2/checkout.h"
#include "repository.h"
#include "posix.h"

static git_repository *g_repo;

void test_checkout_filter_broken__initialize(void)
{
	g_repo = cl_git_sandbox_init("crlf");
}

void test_checkout_filter_broken__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_checkout_filter_broken__bad_content(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_repository_set_head(g_repo, "refs/heads/bad-content");
	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/test1.txt", "");

	if (GIT_EOL_NATIVE == GIT_EOL_LF)
		check_file_contents("./crlf/test2.txt", "test2.txt's content\n");
	else
		check_file_contents("./crlf/test2.txt", "test2.txt's content\r\n");

	/* this will fail with core.autocrlf true and running it on Windows */
	check_file_contents("./crlf/test3.txt", "");
}

