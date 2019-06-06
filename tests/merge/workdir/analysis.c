// NOTE: this is essentially duplicated with tests/merge/trees/analysis.c
// You probably want to make changes to both files.

#include "clar_libgit2.h"
#include "git2/repository.h"
#include "../analysis.h"

static git_repository *repo;
static git_index *repo_index;

#define TEST_REPO_PATH "merge-resolve"
#define TEST_INDEX_PATH TEST_REPO_PATH "/.git/index"


/* Fixture setup and teardown */
void test_merge_workdir_analysis__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);

    testimpl_merge_analysis__initialize(repo, repo_index);
}

void test_merge_workdir_analysis__cleanup(void)
{
    testimpl_merge_analysis__cleanup();

	git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

void test_merge_workdir_analysis__fastforward(void)
{
    testimpl_merge_analysis__fastforward();
}

void test_merge_workdir_analysis__no_fastforward(void)
{
    testimpl_merge_analysis__no_fastforward();
}

void test_merge_workdir_analysis__uptodate(void)
{
    testimpl_merge_analysis__uptodate();
}

void test_merge_workdir_analysis__uptodate_merging_prev_commit(void)
{
    testimpl_merge_analysis__uptodate_merging_prev_commit();
}

void test_merge_workdir_analysis__unborn(void)
{
    testimpl_merge_analysis__unborn();
}

void test_merge_workdir_analysis__fastforward_with_config_noff(void)
{
    testimpl_merge_analysis__fastforward_with_config_noff();
}

void test_merge_workdir_analysis__no_fastforward_with_config_ffonly(void)
{
    testimpl_merge_analysis__no_fastforward_with_config_ffonly();
}

void test_merge_workdir_analysis__between_uptodate_refs(void)
{
    testimpl_merge_analysis__between_uptodate_refs();
}

void test_merge_workdir_analysis__between_noff_refs(void)
{
    testimpl_merge_analysis__between_noff_refs();
}
