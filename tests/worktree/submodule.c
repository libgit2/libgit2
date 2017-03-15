#include "clar_libgit2.h"
#include "repository.h"
#include "worktree_helpers.h"

#define WORKTREE_PARENT "submodules-worktree-parent"
#define WORKTREE_CHILD "submodules-worktree-child"

static worktree_fixture parent
    = WORKTREE_FIXTURE_INIT("submodules", WORKTREE_PARENT);
static worktree_fixture child
    = WORKTREE_FIXTURE_INIT(NULL, WORKTREE_CHILD);

void test_worktree_submodule__initialize(void)
{
	setup_fixture_worktree(&parent);

	cl_git_pass(p_rename(
		"submodules/testrepo/.gitted",
		"submodules/testrepo/.git"));

	setup_fixture_worktree(&child);
}

void test_worktree_submodule__cleanup(void)
{
	cleanup_fixture_worktree(&child);
	cleanup_fixture_worktree(&parent);
}

void test_worktree_submodule__submodule_worktree_parent(void)
{
	cl_assert(git_repository_path(parent.worktree) != NULL);
	cl_assert(git_repository_workdir(parent.worktree) != NULL);

	cl_assert(!parent.repo->is_worktree);
	cl_assert(parent.worktree->is_worktree);
}

void test_worktree_submodule__submodule_worktree_child(void)
{
	cl_assert(!parent.repo->is_worktree);
	cl_assert(parent.worktree->is_worktree);
	cl_assert(child.worktree->is_worktree);
}

void test_worktree_submodule__open_discovered_submodule_worktree(void)
{
	git_buf path = GIT_BUF_INIT;
	git_repository *repo;

	cl_git_pass(git_repository_discover(&path,
		git_repository_workdir(child.worktree), false, NULL));
	cl_git_pass(git_repository_open(&repo, path.ptr));
	cl_assert_equal_s(git_repository_workdir(child.worktree),
		git_repository_workdir(repo));

	git_buf_free(&path);
	git_repository_free(repo);
}
