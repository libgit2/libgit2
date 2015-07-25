#include "clar_libgit2.h"
#include "repository.h"
#include "../submodule/submodule_helpers.h"

static git_repository *g_repo;
static git_index *g_idx;

void test_index_bypath__initialize(void)
{
	g_repo = setup_fixture_submod2();
	cl_git_pass(git_repository_index__weakptr(&g_idx, g_repo));
}

void test_index_bypath__cleanup(void)
{
	g_repo = NULL;
	g_idx = NULL;
}

void test_index_bypath__add_directory(void)
{
	cl_git_fail_with(GIT_EDIRECTORY, git_index_add_bypath(g_idx, "just_a_dir"));
}

void test_index_bypath__add_submodule(void)
{
	unsigned int status;
	const char *sm_name = "sm_changed_head";

	cl_git_pass(git_submodule_status(&status, g_repo, sm_name, 0));
	cl_assert_equal_i(GIT_SUBMODULE_STATUS_WD_MODIFIED, status & GIT_SUBMODULE_STATUS_WD_MODIFIED);
	cl_git_pass(git_index_add_bypath(g_idx, sm_name));
	cl_git_pass(git_submodule_status(&status, g_repo, sm_name, 0));
	cl_assert_equal_i(0, status & GIT_SUBMODULE_STATUS_WD_MODIFIED);
}

void test_index_bypath__add_not_submodule(void)
{
	const char *sm_name = "not-submodule";

	cl_git_fail_with(GIT_EEXISTS, git_index_add_bypath(g_idx, sm_name));
}
