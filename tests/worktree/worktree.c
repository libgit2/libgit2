#include "clar_libgit2.h"
#include "worktree_helpers.h"

#include "repository.h"
#include "worktree.h"

#define COMMON_REPO "testrepo"
#define WORKTREE_REPO "testrepo-worktree"

static worktree_fixture fixture =
	WORKTREE_FIXTURE_INIT(COMMON_REPO, WORKTREE_REPO);

void test_worktree_worktree__initialize(void)
{
	setup_fixture_worktree(&fixture);
}

void test_worktree_worktree__cleanup(void)
{
	cleanup_fixture_worktree(&fixture);
}

void test_worktree_worktree__list(void)
{
	git_strarray wts;

	cl_git_pass(git_worktree_list(&wts, fixture.repo));
	cl_assert_equal_i(wts.count, 1);
	cl_assert_equal_s(wts.strings[0], "testrepo-worktree");

	git_strarray_free(&wts);
}

void test_worktree_worktree__list_with_invalid_worktree_dirs(void)
{
	const char *filesets[3][2] = {
		{ "gitdir", "commondir" },
		{ "gitdir", "HEAD" },
		{ "HEAD", "commondir" },
	};
	git_buf path = GIT_BUF_INIT;
	git_strarray wts;
	unsigned i, j, len;

	cl_git_pass(git_buf_printf(&path, "%s/worktrees/invalid",
		    fixture.repo->commondir));
	cl_git_pass(p_mkdir(path.ptr, 0755));

	len = path.size;

	for (i = 0; i < ARRAY_SIZE(filesets); i++) {

		for (j = 0; j < ARRAY_SIZE(filesets[i]); j++) {
			git_buf_truncate(&path, len);
			cl_git_pass(git_buf_joinpath(&path, path.ptr, filesets[i][j]));
			cl_git_pass(p_close(p_creat(path.ptr, 0644)));
		}

		cl_git_pass(git_worktree_list(&wts, fixture.worktree));
		cl_assert_equal_i(wts.count, 1);
		cl_assert_equal_s(wts.strings[0], "testrepo-worktree");
		git_strarray_free(&wts);

		for (j = 0; j < ARRAY_SIZE(filesets[i]); j++) {
			git_buf_truncate(&path, len);
			cl_git_pass(git_buf_joinpath(&path, path.ptr, filesets[i][j]));
			p_unlink(path.ptr);
		}
	}

	git_buf_free(&path);
}

void test_worktree_worktree__list_in_worktree_repo(void)
{
	git_strarray wts;

	cl_git_pass(git_worktree_list(&wts, fixture.worktree));
	cl_assert_equal_i(wts.count, 1);
	cl_assert_equal_s(wts.strings[0], "testrepo-worktree");

	git_strarray_free(&wts);
}

void test_worktree_worktree__list_bare(void)
{
	git_repository *repo;
	git_strarray wts;

	repo = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_worktree_list(&wts, repo));
	cl_assert_equal_i(wts.count, 0);

	git_repository_free(repo);
}

void test_worktree_worktree__list_without_worktrees(void)
{
	git_repository *repo;
	git_strarray wts;

	repo = cl_git_sandbox_init("testrepo2");
	cl_git_pass(git_worktree_list(&wts, repo));
	cl_assert_equal_i(wts.count, 0);

	git_repository_free(repo);
}

void test_worktree_worktree__lookup(void)
{
	git_worktree *wt;
	git_buf gitdir_path = GIT_BUF_INIT;

	cl_git_pass(git_worktree_lookup(&wt, fixture.repo, "testrepo-worktree"));

	git_buf_printf(&gitdir_path, "%s/worktrees/%s", fixture.repo->commondir, "testrepo-worktree");

	cl_assert_equal_s(wt->gitdir_path, gitdir_path.ptr);
	cl_assert_equal_s(wt->parent_path, fixture.repo->path_repository);
	cl_assert_equal_s(wt->gitlink_path, fixture.worktree->path_gitlink);
	cl_assert_equal_s(wt->commondir_path, fixture.repo->commondir);

	git_buf_free(&gitdir_path);
	git_worktree_free(wt);
}

void test_worktree_worktree__lookup_nonexistent_worktree(void)
{
	git_worktree *wt;

	cl_git_fail(git_worktree_lookup(&wt, fixture.repo, "nonexistent"));
	cl_assert_equal_p(wt, NULL);
}
