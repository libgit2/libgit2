#include "clar_libgit2.h"
#include "buffer.h"
#include "path.h"
#include "posix.h"
#include "status_helpers.h"
#include "../submodule/submodule_helpers.h"

static git_repository *g_repo = NULL;

void test_status_submodules__initialize(void)
{
	g_repo = cl_git_sandbox_init("submodules");

	cl_fixture_sandbox("testrepo.git");

	rewrite_gitmodules(git_repository_workdir(g_repo));

	p_rename("submodules/testrepo/.gitted", "submodules/testrepo/.git");
}

void test_status_submodules__cleanup(void)
{
	cl_git_sandbox_cleanup();
	cl_fixture_cleanup("testrepo.git");
}

void test_status_submodules__api(void)
{
	git_submodule *sm;

	cl_assert(git_submodule_lookup(NULL, g_repo, "nonexistent") == GIT_ENOTFOUND);

	cl_assert(git_submodule_lookup(NULL, g_repo, "modified") == GIT_ENOTFOUND);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "testrepo"));
	cl_assert(sm != NULL);
	cl_assert_equal_s("testrepo", git_submodule_name(sm));
	cl_assert_equal_s("testrepo", git_submodule_path(sm));
}

void test_status_submodules__0(void)
{
	int counts = 0;

	cl_assert(git_path_isdir("submodules/.git"));
	cl_assert(git_path_isdir("submodules/testrepo/.git"));
	cl_assert(git_path_isfile("submodules/.gitmodules"));

	cl_git_pass(
		git_status_foreach(g_repo, cb_status__count, &counts)
	);

	cl_assert_equal_i(6, counts);
}

static const char *expected_files[] = {
	".gitmodules",
	"added",
	"deleted",
	"ignored",
	"modified",
	"untracked"
};

static unsigned int expected_status[] = {
	GIT_STATUS_WT_MODIFIED,
	GIT_STATUS_INDEX_NEW,
	GIT_STATUS_INDEX_DELETED,
	GIT_STATUS_IGNORED,
	GIT_STATUS_WT_MODIFIED,
	GIT_STATUS_WT_NEW
};

static int
cb_status__match(const char *p, unsigned int s, void *payload)
{
	volatile int *index = (int *)payload;

	cl_assert_equal_s(expected_files[*index], p);
	cl_assert(expected_status[*index] == s);
	(*index)++;

	return 0;
}

void test_status_submodules__1(void)
{
	int index = 0;

	cl_assert(git_path_isdir("submodules/.git"));
	cl_assert(git_path_isdir("submodules/testrepo/.git"));
	cl_assert(git_path_isfile("submodules/.gitmodules"));

	cl_git_pass(
		git_status_foreach(g_repo, cb_status__match, &index)
	);

	cl_assert_equal_i(6, index);
}

void test_status_submodules__single_file(void)
{
	unsigned int status = 0;
	cl_git_pass( git_status_file(&status, g_repo, "testrepo") );
	cl_assert(!status);
}
