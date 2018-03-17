#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/blame.h"
#include "git2/mailmap.h"
#include "mailmap_helpers.h"

static git_repository *g_repo;
static git_blame *g_blame;
static git_mailmap *g_mailmap;

void test_mailmap_blame__initialize(void)
{
	g_repo = NULL;
	g_blame = NULL;
}

void test_mailmap_blame__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;

	git_blame_free(g_blame);
	g_blame = NULL;
}

void test_mailmap_blame__hunks(void)
{
	size_t idx = 0;
	const git_blame_hunk *hunk = NULL;
	git_blame_options opts = GIT_BLAME_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("mailmap");

	opts.flags |= GIT_BLAME_USE_MAILMAP;

	cl_check_pass(git_blame_file(&g_blame, g_repo, "file.txt", &opts));
	if (!g_blame)
		return;

	for (idx = 0; idx < ARRAY_SIZE(resolved); ++idx) {
		hunk = git_blame_get_hunk_byline(g_blame, idx + 1);

		cl_assert(hunk->final_signature != NULL);
		cl_assert(hunk->orig_signature != NULL);
		cl_assert_equal_s(hunk->final_signature->name, resolved[idx].real_name);
		cl_assert_equal_s(hunk->final_signature->email, resolved[idx].real_email);
	}
}

void test_mailmap_blame__hunks_no_mailmap(void)
{
	size_t idx = 0;
	const git_blame_hunk *hunk = NULL;
	git_blame_options opts = GIT_BLAME_OPTIONS_INIT;

	g_repo = cl_git_sandbox_init("mailmap");

	cl_check_pass(git_blame_file(&g_blame, g_repo, "file.txt", &opts));
	if (!g_blame)
		return;

	for (idx = 0; idx < ARRAY_SIZE(resolved); ++idx) {
		hunk = git_blame_get_hunk_byline(g_blame, idx + 1);

		cl_assert(hunk->final_signature != NULL);
		cl_assert(hunk->orig_signature != NULL);
		cl_assert_equal_s(hunk->final_signature->name, resolved[idx].replace_name);
		cl_assert_equal_s(hunk->final_signature->email, resolved[idx].replace_email);
	}
}
