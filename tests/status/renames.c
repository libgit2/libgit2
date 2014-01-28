#include "clar_libgit2.h"
#include "buffer.h"
#include "path.h"
#include "posix.h"
#include "status_helpers.h"
#include "util.h"
#include "status.h"

static git_repository *g_repo = NULL;

void test_status_renames__initialize(void)
{
	g_repo = cl_git_sandbox_init("renames");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);
}

void test_status_renames__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void rename_file(git_repository *repo, const char *oldname, const char *newname)
{
	git_buf oldpath = GIT_BUF_INIT, newpath = GIT_BUF_INIT;

	git_buf_joinpath(&oldpath, git_repository_workdir(repo), oldname);
	git_buf_joinpath(&newpath, git_repository_workdir(repo), newname);

	cl_git_pass(p_rename(oldpath.ptr, newpath.ptr));

	git_buf_free(&oldpath);
	git_buf_free(&newpath);
}

static void rename_and_edit_file(git_repository *repo, const char *oldname, const char *newname)
{
	git_buf oldpath = GIT_BUF_INIT, newpath = GIT_BUF_INIT;

	git_buf_joinpath(&oldpath, git_repository_workdir(repo), oldname);
	git_buf_joinpath(&newpath, git_repository_workdir(repo), newname);

	cl_git_pass(p_rename(oldpath.ptr, newpath.ptr));
	cl_git_append2file(newpath.ptr, "Added at the end to keep similarity!");

	git_buf_free(&oldpath);
	git_buf_free(&newpath);
}

struct status_entry {
	git_status_t status;
	const char *oldname;
	const char *newname;
};

static void test_status(
	git_status_list *status_list,
	struct status_entry *expected_list,
	size_t expected_len)
{
	const git_status_entry *actual;
	const struct status_entry *expected;
	const char *oldname, *newname;
	size_t i;

	cl_assert_equal_sz(expected_len, git_status_list_entrycount(status_list));

	for (i = 0; i < expected_len; i++) {
		actual = git_status_byindex(status_list, i);
		expected = &expected_list[i];

		oldname = actual->head_to_index ? actual->head_to_index->old_file.path :
			actual->index_to_workdir ? actual->index_to_workdir->old_file.path : NULL;

		newname = actual->index_to_workdir ? actual->index_to_workdir->new_file.path :
			actual->head_to_index ? actual->head_to_index->new_file.path : NULL;

		cl_assert_equal_i_fmt(expected->status, actual->status, "%04x");

		if (oldname)
			cl_assert(git__strcmp(oldname, expected->oldname) == 0);
		else
			cl_assert(expected->oldname == NULL);

		if (newname)
			cl_assert(git__strcmp(newname, expected->newname) == 0);
		else
			cl_assert(expected->newname == NULL);
	}
}

void test_status_renames__head2index_one(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED, "ikeepsix.txt", "newname.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "newname.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "newname.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 1);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__head2index_two(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED,
		  "sixserving.txt", "aaa.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED,
		  "untimely.txt", "bbb.txt" },
		{ GIT_STATUS_INDEX_RENAMED, "songof7cities.txt", "ccc.txt" },
		{ GIT_STATUS_INDEX_RENAMED, "ikeepsix.txt", "ddd.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "ddd.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "aaa.txt");
	rename_file(g_repo, "songof7cities.txt", "ccc.txt");
	rename_and_edit_file(g_repo, "untimely.txt", "bbb.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_remove_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_remove_bypath(index, "songof7cities.txt"));
	cl_git_pass(git_index_remove_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_add_bypath(index, "ddd.txt"));
	cl_git_pass(git_index_add_bypath(index, "aaa.txt"));
	cl_git_pass(git_index_add_bypath(index, "ccc.txt"));
	cl_git_pass(git_index_add_bypath(index, "bbb.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 4);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__head2index_no_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_MODIFIED, "ikeepsix.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_MODIFIED, "sixserving.txt", "sixserving.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_index_add_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 2);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__head2index_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED, "sixserving.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_RENAMED, "ikeepsix.txt", "sixserving.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_index_add_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 2);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__index2workdir_one(void)
{
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_RENAMED, "ikeepsix.txt", "newname.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	rename_file(g_repo, "ikeepsix.txt", "newname.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 1);
	git_status_list_free(statuslist);
}

void test_status_renames__index2workdir_two(void)
{
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "sixserving.txt", "aaa.txt" },
		{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "untimely.txt", "bbb.txt" },
		{ GIT_STATUS_WT_RENAMED, "songof7cities.txt", "ccc.txt" },
		{ GIT_STATUS_WT_RENAMED, "ikeepsix.txt", "ddd.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	rename_file(g_repo, "ikeepsix.txt", "ddd.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "aaa.txt");
	rename_file(g_repo, "songof7cities.txt", "ccc.txt");
	rename_and_edit_file(g_repo, "untimely.txt", "bbb.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 4);
	git_status_list_free(statuslist);
}

void test_status_renames__index2workdir_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_RENAMED, "sixserving.txt", "ikeepsix.txt" },
		{ GIT_STATUS_WT_RENAMED, "ikeepsix.txt", "sixserving.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 2);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_one(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "ikeepsix.txt", "newname-workdir.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "newname-index.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "newname-index.txt"));
	cl_git_pass(git_index_write(index));

	rename_file(g_repo, "newname-index.txt", "newname-workdir.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 1);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_two(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED |
		  GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "ikeepsix.txt", "ikeepsix-both.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED,
		  "sixserving.txt", "sixserving-index.txt" },
		{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "songof7cities.txt", "songof7cities-workdir.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "untimely.txt", "untimely-both.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_and_edit_file(g_repo, "ikeepsix.txt", "ikeepsix-index.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "sixserving-index.txt");
	rename_file(g_repo, "untimely.txt", "untimely-index.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_remove_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_remove_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_add_bypath(index, "ikeepsix-index.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving-index.txt"));
	cl_git_pass(git_index_add_bypath(index, "untimely-index.txt"));
	cl_git_pass(git_index_write(index));

	rename_and_edit_file(g_repo, "ikeepsix-index.txt", "ikeepsix-both.txt");
	rename_and_edit_file(g_repo, "songof7cities.txt", "songof7cities-workdir.txt");
	rename_file(g_repo, "untimely-index.txt", "untimely-both.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 4);
	git_status_list_free(statuslist);

	git_index_free(index);
}


void test_status_renames__both_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "songof7cities.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "ikeepsix.txt", "sixserving.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "sixserving.txt", "songof7cities.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "songof7cities.txt", "sixserving.txt");
	rename_file(g_repo, "_temp_.txt", "songof7cities.txt");

	cl_git_pass(git_index_add_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_add_bypath(index, "songof7cities.txt"));
	cl_git_pass(git_index_write(index));

	rename_file(g_repo, "songof7cities.txt", "_temp_.txt");
	rename_file(g_repo, "ikeepsix.txt", "songof7cities.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 3);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__rewrites_only_for_renames(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_MODIFIED, "ikeepsix.txt", "ikeepsix.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	cl_git_rewritefile("renames/ikeepsix.txt",
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 1);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_casechange_one(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	int index_caps;
	struct status_entry expected_icase[] = {
		{ GIT_STATUS_INDEX_RENAMED,
		  "ikeepsix.txt", "IKeepSix.txt" },
	};
	struct status_entry expected_case[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "ikeepsix.txt", "IKEEPSIX.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));
	index_caps = git_index_caps(index);

	rename_file(g_repo, "ikeepsix.txt", "IKeepSix.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "IKeepSix.txt"));
	cl_git_pass(git_index_write(index));

	/* on a case-insensitive file system, this change won't matter.
	 * on a case-sensitive one, it will.
	 */
	rename_file(g_repo, "IKeepSix.txt", "IKEEPSIX.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));

	test_status(statuslist, (index_caps & GIT_INDEXCAP_IGNORE_CASE) ?
		expected_icase : expected_case, 1);

	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_casechange_two(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	int index_caps;
	struct status_entry expected_icase[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED |
		  GIT_STATUS_WT_MODIFIED,
		  "ikeepsix.txt", "IKeepSix.txt" },
		{ GIT_STATUS_INDEX_MODIFIED,
		  "sixserving.txt", "sixserving.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "songof7cities.txt", "songof7.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "untimely.txt", "untimeliest.txt" }
	};
	struct status_entry expected_case[] = {
		{ GIT_STATUS_INDEX_RENAMED |
		  GIT_STATUS_WT_MODIFIED | GIT_STATUS_WT_RENAMED,
		  "songof7cities.txt", "SONGOF7.txt" },
		{ GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_RENAMED,
		  "sixserving.txt", "SixServing.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED |
		  GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "ikeepsix.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "untimely.txt", "untimeliest.txt" }
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));
	index_caps = git_index_caps(index);

	rename_and_edit_file(g_repo, "ikeepsix.txt", "IKeepSix.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "sixserving.txt");
	rename_file(g_repo, "songof7cities.txt", "songof7.txt");
	rename_file(g_repo, "untimely.txt", "untimelier.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_remove_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_remove_bypath(index, "songof7cities.txt"));
	cl_git_pass(git_index_remove_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_add_bypath(index, "IKeepSix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_add_bypath(index, "songof7.txt"));
	cl_git_pass(git_index_add_bypath(index, "untimelier.txt"));
	cl_git_pass(git_index_write(index));

	rename_and_edit_file(g_repo, "IKeepSix.txt", "ikeepsix.txt");
	rename_file(g_repo, "sixserving.txt", "SixServing.txt");
	rename_and_edit_file(g_repo, "songof7.txt", "SONGOF7.txt");
	rename_file(g_repo, "untimelier.txt", "untimeliest.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));

	test_status(statuslist, (index_caps & GIT_INDEXCAP_IGNORE_CASE) ?
		expected_icase : expected_case, 4);

	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__zero_byte_file_does_not_fail(void)
{
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;

	struct status_entry expected[] = {
		{ GIT_STATUS_WT_DELETED, "ikeepsix.txt", "ikeepsix.txt" },
		{ GIT_STATUS_WT_NEW, "zerobyte.txt", "zerobyte.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES |
		GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
		GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR |
		GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
		GIT_STATUS_SHOW_INDEX_AND_WORKDIR |
		GIT_STATUS_OPT_RECURSE_IGNORED_DIRS;

	p_unlink("renames/ikeepsix.txt");
	cl_git_mkfile("renames/zerobyte.txt", "");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	test_status(statuslist, expected, 2);
	git_status_list_free(statuslist);
}
