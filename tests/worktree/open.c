#include "clar_libgit2.h"
#include "repository.h"
#include "worktree_helpers.h"

#define WORKTREE_PARENT "submodules-worktree-parent"
#define WORKTREE_CHILD "submodules-worktree-child"

#define COMMON_REPO "testrepo"
#define WORKTREE_REPO "testrepo-worktree"

void test_worktree_open__repository(void)
{
	worktree_fixture fixture =
		WORKTREE_FIXTURE_INIT(COMMON_REPO, WORKTREE_REPO);
	setup_fixture_worktree(&fixture);

	cl_assert(git_repository_path(fixture.worktree) != NULL);
	cl_assert(git_repository_workdir(fixture.worktree) != NULL);

	cl_assert(!fixture.repo->is_worktree);
	cl_assert(fixture.worktree->is_worktree);

	cleanup_fixture_worktree(&fixture);
}

void test_worktree_open__open_discovered_worktree(void)
{
	worktree_fixture fixture =
		WORKTREE_FIXTURE_INIT(COMMON_REPO, WORKTREE_REPO);
	git_buf path = GIT_BUF_INIT;
	git_repository *repo;

	setup_fixture_worktree(&fixture);

	cl_git_pass(git_repository_discover(&path,
		git_repository_workdir(fixture.worktree), false, NULL));
	cl_git_pass(git_repository_open(&repo, path.ptr));
	cl_assert_equal_s(git_repository_workdir(fixture.worktree),
		git_repository_workdir(repo));

	git_buf_free(&path);
	git_repository_free(repo);
	cleanup_fixture_worktree(&fixture);
}

void test_worktree_open__repository_with_nonexistent_parent(void)
{
	git_repository *repo;

	cl_fixture_sandbox(WORKTREE_REPO);
	cl_git_pass(p_chdir(WORKTREE_REPO));
	cl_git_pass(cl_rename(".gitted", ".git"));
	cl_git_pass(p_chdir(".."));

	cl_git_fail(git_repository_open(&repo, WORKTREE_REPO));

	cl_fixture_cleanup(WORKTREE_REPO);
}

void test_worktree_open__submodule_worktree_parent(void)
{
	worktree_fixture fixture =
		WORKTREE_FIXTURE_INIT("submodules", WORKTREE_PARENT);
	setup_fixture_worktree(&fixture);

	cl_assert(git_repository_path(fixture.worktree) != NULL);
	cl_assert(git_repository_workdir(fixture.worktree) != NULL);

	cl_assert(!fixture.repo->is_worktree);
	cl_assert(fixture.worktree->is_worktree);

	cleanup_fixture_worktree(&fixture);
}

void test_worktree_open__submodule_worktree_child(void)
{
	worktree_fixture parent_fixture =
		WORKTREE_FIXTURE_INIT("submodules", WORKTREE_PARENT);
	worktree_fixture child_fixture =
		WORKTREE_FIXTURE_INIT(NULL, WORKTREE_CHILD);

	setup_fixture_worktree(&parent_fixture);
	cl_git_pass(p_rename(
		"submodules/testrepo/.gitted",
		"submodules/testrepo/.git"));
	setup_fixture_worktree(&child_fixture);

	cl_assert(!parent_fixture.repo->is_worktree);
	cl_assert(parent_fixture.worktree->is_worktree);
	cl_assert(child_fixture.worktree->is_worktree);

	cleanup_fixture_worktree(&child_fixture);
	cleanup_fixture_worktree(&parent_fixture);
}

void test_worktree_open__open_discovered_submodule_worktree(void)
{
	worktree_fixture parent_fixture =
		WORKTREE_FIXTURE_INIT("submodules", WORKTREE_PARENT);
	worktree_fixture child_fixture =
		WORKTREE_FIXTURE_INIT(NULL, WORKTREE_CHILD);
	git_buf path = GIT_BUF_INIT;
	git_repository *repo;

	setup_fixture_worktree(&parent_fixture);
	cl_git_pass(p_rename(
		"submodules/testrepo/.gitted",
		"submodules/testrepo/.git"));
	setup_fixture_worktree(&child_fixture);

	cl_git_pass(git_repository_discover(&path,
		git_repository_workdir(child_fixture.worktree), false, NULL));
	cl_git_pass(git_repository_open(&repo, path.ptr));
	cl_assert_equal_s(git_repository_workdir(child_fixture.worktree),
		git_repository_workdir(repo));

	git_buf_free(&path);
	git_repository_free(repo);
	cleanup_fixture_worktree(&child_fixture);
	cleanup_fixture_worktree(&parent_fixture);
}
