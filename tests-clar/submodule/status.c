#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"
#include "submodule_helpers.h"
#include "fileops.h"

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
	unsigned int status, expected;
	git_submodule *sm;

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	expected = GIT_SUBMODULE_STATUS_IN_HEAD |
		GIT_SUBMODULE_STATUS_IN_INDEX |
		GIT_SUBMODULE_STATUS_IN_CONFIG |
		GIT_SUBMODULE_STATUS_IN_WD;

	cl_assert(status == expected);
}

/* 4 values of GIT_SUBMODULE_IGNORE to check */

void test_submodule_status__ignore_none(void)
{
	unsigned int status;
	git_submodule *sm;
	git_buf path = GIT_BUF_INIT;

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "sm_unchanged"));
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), NULL, GIT_RMDIR_REMOVE_FILES));

	cl_git_fail(git_submodule_lookup(&sm, g_repo, "not_submodule"));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_index"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_INDEX_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_untracked_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_UNTRACKED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_missing_commits"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_added_and_uncommited"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_INDEX_ADDED) != 0);

	/* removed sm_unchanged for deleted workdir */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_DELETED) != 0);

	/* now mkdir sm_unchanged to test uninitialized */
	cl_git_pass(git_futils_mkdir(git_buf_cstr(&path), NULL, 0755, 0));
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_reload(sm));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_UNINITIALIZED) != 0);

	/* update sm_changed_head in index */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_add_to_index(sm, true));
	/* reload is not needed because add_to_index updates the submodule data */
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_INDEX_MODIFIED) != 0);

	/* remove sm_changed_head from index */
	{
		git_index *index;
		size_t pos;

		cl_git_pass(git_repository_index(&index, g_repo));
		cl_assert(!git_index_find(&pos, index, "sm_changed_head"));
		cl_git_pass(git_index_remove(index, "sm_changed_head", 0));
		cl_git_pass(git_index_write(index));

		git_index_free(index);
	}

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_reload(sm));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_INDEX_DELETED) != 0);

	git_buf_free(&path);
}

static int set_sm_ignore(git_submodule *sm, const char *name, void *payload)
{
	git_submodule_ignore_t ignore = *(git_submodule_ignore_t *)payload;
	GIT_UNUSED(name);
	git_submodule_set_ignore(sm, ignore);
	return 0;
}

void test_submodule_status__ignore_untracked(void)
{
	unsigned int status;
	git_submodule *sm;
	git_buf path = GIT_BUF_INIT;
	git_submodule_ignore_t ign = GIT_SUBMODULE_IGNORE_UNTRACKED;

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "sm_unchanged"));
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), NULL, GIT_RMDIR_REMOVE_FILES));

	cl_git_pass(git_submodule_foreach(g_repo, set_sm_ignore, &ign));

	cl_git_fail(git_submodule_lookup(&sm, g_repo, "not_submodule"));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_index"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_INDEX_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_untracked_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_missing_commits"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_added_and_uncommited"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_INDEX_ADDED) != 0);

	/* removed sm_unchanged for deleted workdir */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_DELETED) != 0);

	/* now mkdir sm_unchanged to test uninitialized */
	cl_git_pass(git_futils_mkdir(git_buf_cstr(&path), NULL, 0755, 0));
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_reload(sm));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_UNINITIALIZED) != 0);

	/* update sm_changed_head in index */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_add_to_index(sm, true));
	/* reload is not needed because add_to_index updates the submodule data */
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_INDEX_MODIFIED) != 0);

	git_buf_free(&path);
}

void test_submodule_status__ignore_dirty(void)
{
	unsigned int status;
	git_submodule *sm;
	git_buf path = GIT_BUF_INIT;
	git_submodule_ignore_t ign = GIT_SUBMODULE_IGNORE_DIRTY;

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "sm_unchanged"));
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), NULL, GIT_RMDIR_REMOVE_FILES));

	cl_git_pass(git_submodule_foreach(g_repo, set_sm_ignore, &ign));

	cl_git_fail(git_submodule_lookup(&sm, g_repo, "not_submodule"));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_index"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_untracked_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_missing_commits"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_MODIFIED) != 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_added_and_uncommited"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_INDEX_ADDED) != 0);

	/* removed sm_unchanged for deleted workdir */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_DELETED) != 0);

	/* now mkdir sm_unchanged to test uninitialized */
	cl_git_pass(git_futils_mkdir(git_buf_cstr(&path), NULL, 0755, 0));
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_reload(sm));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_WD_UNINITIALIZED) != 0);

	/* update sm_changed_head in index */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_add_to_index(sm, true));
	/* reload is not needed because add_to_index updates the submodule data */
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert((status & GIT_SUBMODULE_STATUS_INDEX_MODIFIED) != 0);

	git_buf_free(&path);
}

void test_submodule_status__ignore_all(void)
{
	unsigned int status;
	git_submodule *sm;
	git_buf path = GIT_BUF_INIT;
	git_submodule_ignore_t ign = GIT_SUBMODULE_IGNORE_ALL;

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "sm_unchanged"));
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), NULL, GIT_RMDIR_REMOVE_FILES));

	cl_git_pass(git_submodule_foreach(g_repo, set_sm_ignore, &ign));

	cl_git_fail(git_submodule_lookup(&sm, g_repo, "not_submodule"));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_index"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_untracked_file"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_missing_commits"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_added_and_uncommited"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	/* removed sm_unchanged for deleted workdir */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	/* now mkdir sm_unchanged to test uninitialized */
	cl_git_pass(git_futils_mkdir(git_buf_cstr(&path), NULL, 0755, 0));
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_reload(sm));
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	/* update sm_changed_head in index */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_git_pass(git_submodule_add_to_index(sm, true));
	/* reload is not needed because add_to_index updates the submodule data */
	cl_git_pass(git_submodule_status(&status, sm));
	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	git_buf_free(&path);
}
