#include "clar_libgit2.h"
#include "index.h"
#include "git2/repository.h"

static git_repository *repo;
static git_index *repo_index;

#define TEST_REPO_PATH "mergedrepo"
#define TEST_INDEX_PATH TEST_REPO_PATH "/.git/index"

// Fixture setup and teardown
void test_index_stage__initialize(void)
{
	repo = cl_git_sandbox_init("mergedrepo");
	git_repository_index(&repo_index, repo);
}

void test_index_stage__cleanup(void)
{
	git_index_free(repo_index);
	repo_index = NULL;

	cl_git_sandbox_cleanup();
}


void test_index_stage__add_always_adds_stage_0(void)
{
	int entry_idx;
	git_index_entry *entry;

    cl_git_mkfile("./mergedrepo/new-file.txt", "new-file\n");

	cl_git_pass(git_index_add_from_workdir(repo_index, "new-file.txt"));

	cl_assert((entry_idx = git_index_find(repo_index, "new-file.txt")) >= 0);
	cl_assert((entry = git_index_get_byindex(repo_index, entry_idx)) != NULL);
	cl_assert(git_index_entry_stage(entry) == 0);
}

void test_index_stage__find_gets_first_stage(void)
{
	int entry_idx;
	git_index_entry *entry;

	cl_assert((entry_idx = git_index_find(repo_index, "one.txt")) >= 0);
	cl_assert((entry = git_index_get_byindex(repo_index, entry_idx)) != NULL);
	cl_assert(git_index_entry_stage(entry) == 0);

	cl_assert((entry_idx = git_index_find(repo_index, "two.txt")) >= 0);
	cl_assert((entry = git_index_get_byindex(repo_index, entry_idx)) != NULL);
	cl_assert(git_index_entry_stage(entry) == 0);

	cl_assert((entry_idx = git_index_find(repo_index, "conflicts-one.txt")) >= 0);
	cl_assert((entry = git_index_get_byindex(repo_index, entry_idx)) != NULL);
	cl_assert(git_index_entry_stage(entry) == 1);

	cl_assert((entry_idx = git_index_find(repo_index, "conflicts-two.txt")) >= 0);
	cl_assert((entry = git_index_get_byindex(repo_index, entry_idx)) != NULL);
	cl_assert(git_index_entry_stage(entry) == 1);
}

