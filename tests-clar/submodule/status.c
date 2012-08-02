#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"
#include "submodule_helpers.h"

static git_repository *g_repo = NULL;

void test_submodule_status__initialize(void)
{
	g_repo = cl_git_sandbox_init("submod2");

	cl_fixture_sandbox("submod2_target");
	p_rename("submod2_target/.gitted", "submod2_target/.git");

	/* must create submod2_target before rewrite so prettify will work */
	rewrite_gitmodules(git_repository_workdir(g_repo));
	p_rename("submod2/not_submodule/.gitted", "submod2/not_submodule/.git");
}

void test_submodule_status__cleanup(void)
{
	cl_git_sandbox_cleanup();
	cl_fixture_cleanup("submod2_target");
}

void test_submodule_status__unchanged(void)
{
	/* make sure it really looks unchanged */
}

void test_submodule_status__changed(void)
{
	/* 4 values of GIT_SUBMODULE_IGNORE to check */

	/* 6 states of change:
	 * - none, (handled in __unchanged above)
	 * - dirty workdir file,
	 * - dirty index,
	 * - moved head,
	 * - untracked file,
	 * - missing commits (i.e. superproject commit is ahead of submodule)
	 */
}

