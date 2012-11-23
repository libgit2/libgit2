#include "clar_libgit2.h"
#include "posix.h"
#include "reset_helpers.h"
#include "path.h"

static git_repository *repo;
static git_object *target;

void test_reset_mixed__initialize(void)
{
	repo = cl_git_sandbox_init("attr");
	target = NULL;
}

void test_reset_mixed__cleanup(void)
{
	git_object_free(target);
	target = NULL;

	cl_git_sandbox_cleanup();
}

void test_reset_mixed__cannot_reset_in_a_bare_repository(void)
{
	git_repository *bare;

	cl_git_pass(git_repository_open(&bare, cl_fixture("testrepo.git")));
	cl_assert(git_repository_is_bare(bare) == true);

	retrieve_target_from_oid(&target, bare, KNOWN_COMMIT_IN_BARE_REPO);

	cl_assert_equal_i(GIT_EBAREREPO, git_reset(bare, target, GIT_RESET_MIXED));

	git_repository_free(bare);
}

void test_reset_mixed__resetting_refreshes_the_index_to_the_commit_tree(void)
{
	unsigned int status;

	cl_git_pass(git_status_file(&status, repo, "macro_bad"));
	cl_assert(status == GIT_STATUS_CURRENT);
	retrieve_target_from_oid(&target, repo, "605812ab7fe421fdd325a935d35cb06a9234a7d7");

	cl_git_pass(git_reset(repo, target, GIT_RESET_MIXED));

	cl_git_pass(git_status_file(&status, repo, "macro_bad"));
	cl_assert(status == GIT_STATUS_WT_NEW);
}
