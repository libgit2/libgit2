#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"
#include "submodule_helpers.h"
#include "fileops.h"
#include "iterator.h"

static git_repository *g_repo = NULL;

void test_submodule_status__initialize(void)
{
	g_repo = setup_fixture_submod2();
}

void test_submodule_status__cleanup(void)
{
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

	cl_assert_equal_i(GIT_ENOTFOUND,
		git_submodule_lookup(&sm, g_repo, "just_a_dir"));
	cl_assert_equal_i(GIT_EEXISTS,
		git_submodule_lookup(&sm, g_repo, "not-submodule"));
	cl_assert_equal_i(GIT_EEXISTS,
		git_submodule_lookup(&sm, g_repo, "not"));

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

	cl_git_fail(git_submodule_lookup(&sm, g_repo, "not-submodule"));

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

	cl_assert_equal_i(GIT_ENOTFOUND,
		git_submodule_lookup(&sm, g_repo, "just_a_dir"));
	cl_assert_equal_i(GIT_EEXISTS,
		git_submodule_lookup(&sm, g_repo, "not-submodule"));
	cl_assert_equal_i(GIT_EEXISTS,
		git_submodule_lookup(&sm, g_repo, "not"));

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

	cl_assert_equal_i(GIT_ENOTFOUND,
		git_submodule_lookup(&sm, g_repo, "just_a_dir"));
	cl_assert_equal_i(GIT_EEXISTS,
		git_submodule_lookup(&sm, g_repo, "not-submodule"));
	cl_assert_equal_i(GIT_EEXISTS,
		git_submodule_lookup(&sm, g_repo, "not"));

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

typedef struct {
	size_t counter;
	const char **paths;
	int *statuses;
} submodule_expectations;

static int confirm_submodule_status(
	const char *path, unsigned int status_flags, void *payload)
{
	submodule_expectations *exp = payload;

	while (git__suffixcmp(exp->paths[exp->counter], "/") == 0)
		exp->counter++;

	cl_assert_equal_i(exp->statuses[exp->counter], (int)status_flags);
	cl_assert_equal_s(exp->paths[exp->counter++], path);

	GIT_UNUSED(status_flags);

	return 0;
}

void test_submodule_status__iterator(void)
{
	git_iterator *iter;
	const git_index_entry *entry;
	size_t i;
	static const char *expected[] = {
		".gitmodules",
		"just_a_dir/",
		"just_a_dir/contents",
		"just_a_file",
		"not",
		"not-submodule",
		"README.txt",
		"sm_added_and_uncommited",
		"sm_changed_file",
		"sm_changed_head",
		"sm_changed_index",
		"sm_changed_untracked_file",
		"sm_missing_commits",
		"sm_unchanged",
		NULL
	};
	static int expected_flags[] = {
		GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_MODIFIED, /* ".gitmodules" */
		0,					    /* "just_a_dir/" will be skipped */
		GIT_STATUS_CURRENT,     /* "just_a_dir/contents" */
		GIT_STATUS_CURRENT,	    /* "just_a_file" */
		GIT_STATUS_IGNORED,	    /* "not" (contains .git) */
		GIT_STATUS_IGNORED,     /* "not-submodule" (contains .git) */
		GIT_STATUS_CURRENT,     /* "README.txt */
		GIT_STATUS_INDEX_NEW,   /* "sm_added_and_uncommited" */
		GIT_STATUS_WT_MODIFIED, /* "sm_changed_file" */
		GIT_STATUS_WT_MODIFIED, /* "sm_changed_head" */
		GIT_STATUS_WT_MODIFIED, /* "sm_changed_index" */
		GIT_STATUS_WT_MODIFIED, /* "sm_changed_untracked_file" */
		GIT_STATUS_WT_MODIFIED, /* "sm_missing_commits" */
		GIT_STATUS_CURRENT,     /* "sm_unchanged" */
		0
	};
	submodule_expectations exp = { 0, expected, expected_flags };
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;

	cl_git_pass(git_iterator_for_workdir(&iter, g_repo,
		GIT_ITERATOR_IGNORE_CASE | GIT_ITERATOR_INCLUDE_TREES, NULL, NULL));

	for (i = 0; !git_iterator_advance(&entry, iter); ++i)
		cl_assert_equal_s(expected[i], entry->path);

	git_iterator_free(iter);

	opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_INCLUDE_UNMODIFIED |
		GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
		GIT_STATUS_OPT_SORT_CASE_INSENSITIVELY;

	cl_git_pass(git_status_foreach_ext(
		g_repo, &opts, confirm_submodule_status, &exp));
}

void test_submodule_status__untracked_dirs_containing_ignored_files(void)
{
	git_buf path = GIT_BUF_INIT;
	unsigned int status, expected;
	git_submodule *sm;

	cl_git_pass(git_buf_joinpath(&path, git_repository_path(g_repo), "modules/sm_unchanged/info/exclude"));
	cl_git_append2file(git_buf_cstr(&path), "\n*.ignored\n");

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "sm_unchanged/directory"));
	cl_git_pass(git_futils_mkdir(git_buf_cstr(&path), NULL, 0755, 0));
	cl_git_pass(git_buf_joinpath(&path, git_buf_cstr(&path), "i_am.ignored"));
	cl_git_mkfile(git_buf_cstr(&path), "ignored this file, please\n");

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_git_pass(git_submodule_status(&status, sm));

	cl_assert(GIT_SUBMODULE_STATUS_IS_UNMODIFIED(status));

	expected = GIT_SUBMODULE_STATUS_IN_HEAD |
		GIT_SUBMODULE_STATUS_IN_INDEX |
		GIT_SUBMODULE_STATUS_IN_CONFIG |
		GIT_SUBMODULE_STATUS_IN_WD;

	cl_assert(status == expected);

	git_buf_free(&path);
}
