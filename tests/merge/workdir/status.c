#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "git2/sys/index.h"
#include "merge.h"
#include "../merge_helpers.h"
#include "refs.h"

static git_repository *repo;
static git_index *repo_index;

#define TEST_REPO_PATH "merge-resolve"
#define TEST_INDEX_PATH TEST_REPO_PATH "/.git/index"

#define UPTODATE_BRANCH			"master"
#define PREVIOUS_BRANCH			"previous"

#define FASTFORWARD_BRANCH		"ff_branch"
#define FASTFORWARD_ID			"fd89f8cffb663ac89095a0f9764902e93ceaca6a"

#define NOFASTFORWARD_BRANCH	"branch"
#define NOFASTFORWARD_ID		"7cb63eed597130ba4abb87b3e544b85021905520"


// Fixture setup and teardown
void test_merge_workdir_status__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);
}

void test_merge_workdir_status__cleanup(void)
{
	git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

static git_status_t status_from_branch(const char *branchname)
{
	git_buf refname = GIT_BUF_INIT;
	git_reference *their_ref;
	git_merge_head *their_heads[1];
	git_status_t status;

	git_buf_printf(&refname, "%s%s", GIT_REFS_HEADS_DIR, branchname);

	cl_git_pass(git_reference_lookup(&their_ref, repo, git_buf_cstr(&refname)));
	cl_git_pass(git_merge_head_from_ref(&their_heads[0], repo, their_ref));

	cl_git_pass(git_merge_status(&status, repo, their_heads, 1));

	git_buf_free(&refname);
	git_merge_head_free(their_heads[0]);
	git_reference_free(their_ref);

	return status;
}

void test_merge_workdir_status__fastforward(void)
{
	git_merge_status_t status;

	status = status_from_branch(FASTFORWARD_BRANCH);
	cl_assert_equal_i(GIT_MERGE_STATUS_FASTFORWARD, status);
}

void test_merge_workdir_status__no_fastforward(void)
{
	git_merge_status_t status;

	status = status_from_branch(NOFASTFORWARD_BRANCH);
	cl_assert_equal_i(GIT_MERGE_STATUS_NORMAL, status);
}

void test_merge_workdir_status__uptodate(void)
{
	git_merge_status_t status;

	status = status_from_branch(UPTODATE_BRANCH);
	cl_assert_equal_i(GIT_MERGE_STATUS_UP_TO_DATE, status);
}

void test_merge_workdir_status__uptodate_merging_prev_commit(void)
{
	git_merge_status_t status;

	status = status_from_branch(PREVIOUS_BRANCH);
	cl_assert_equal_i(GIT_MERGE_STATUS_UP_TO_DATE, status);
}
