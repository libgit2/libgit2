#include "clar_libgit2.h"
#include "fileops.h"
#include "ignore.h"
#include "status_data.h"
#include "posix.h"
#include "util.h"
#include "path.h"

/**
 * Initializer
 *
 * Not all of the tests in this file use the same fixtures, so we allow each
 * test to load their fixture at the top of the test function.
 */
void test_status_worktree__initialize(void)
{
}

/**
 * Cleanup
 *
 * This will be called once after each test finishes, even
 * if the test failed
 */
void test_status_worktree__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

/**
 * Tests - Status determination on a working tree
 */
/* this test is equivalent to t18-status.c:statuscb0 */
void test_status_worktree__whole_repository(void)
{
	status_entry_counts counts;
	git_repository *repo = cl_git_sandbox_init("status");

	memset(&counts, 0x0, sizeof(status_entry_counts));
	counts.expected_entry_count = entry_count0;
	counts.expected_paths = entry_paths0;
	counts.expected_statuses = entry_statuses0;

	cl_git_pass(
		git_status_foreach(repo, cb_status__normal, &counts)
	);

	cl_assert_equal_i(counts.expected_entry_count, counts.entry_count);
	cl_assert_equal_i(0, counts.wrong_status_flags_count);
	cl_assert_equal_i(0, counts.wrong_sorted_path);
}

/* this test is equivalent to t18-status.c:statuscb1 */
void test_status_worktree__empty_repository(void)
{
	int count = 0;
	git_repository *repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_status_foreach(repo, cb_status__count, &count));

	cl_assert_equal_i(0, count);
}

static int remove_file_cb(void *data, git_buf *file)
{
	const char *filename = git_buf_cstr(file);

	GIT_UNUSED(data);

	if (git__suffixcmp(filename, ".git") == 0)
		return 0;

	if (git_path_isdir(filename))
		cl_git_pass(git_futils_rmdir_r(filename, GIT_DIRREMOVAL_FILES_AND_DIRS));
	else
		cl_git_pass(p_unlink(git_buf_cstr(file)));

	return 0;
}

/* this test is equivalent to t18-status.c:statuscb2 */
void test_status_worktree__purged_worktree(void)
{
	status_entry_counts counts;
	git_repository *repo = cl_git_sandbox_init("status");
	git_buf workdir = GIT_BUF_INIT;

	/* first purge the contents of the worktree */
	cl_git_pass(git_buf_sets(&workdir, git_repository_workdir(repo)));
	cl_git_pass(git_path_direach(&workdir, remove_file_cb, NULL));
	git_buf_free(&workdir);

	/* now get status */
	memset(&counts, 0x0, sizeof(status_entry_counts));
	counts.expected_entry_count = entry_count2;
	counts.expected_paths = entry_paths2;
	counts.expected_statuses = entry_statuses2;

	cl_git_pass(
		git_status_foreach(repo, cb_status__normal, &counts)
	);

	cl_assert_equal_i(counts.expected_entry_count, counts.entry_count);
	cl_assert_equal_i(0, counts.wrong_status_flags_count);
	cl_assert_equal_i(0, counts.wrong_sorted_path);
}

/* this test is similar to t18-status.c:statuscb3 */
void test_status_worktree__swap_subdir_and_file(void)
{
	status_entry_counts counts;
	git_repository *repo = cl_git_sandbox_init("status");
	git_status_options opts;

	/* first alter the contents of the worktree */
	cl_git_pass(p_rename("status/current_file", "status/swap"));
	cl_git_pass(p_rename("status/subdir", "status/current_file"));
	cl_git_pass(p_rename("status/swap", "status/subdir"));

	cl_git_mkfile("status/.HEADER", "dummy");
	cl_git_mkfile("status/42-is-not-prime.sigh", "dummy");
	cl_git_mkfile("status/README.md", "dummy");

	/* now get status */
	memset(&counts, 0x0, sizeof(status_entry_counts));
	counts.expected_entry_count = entry_count3;
	counts.expected_paths = entry_paths3;
	counts.expected_statuses = entry_statuses3;

	memset(&opts, 0, sizeof(opts));
	opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_INCLUDE_IGNORED;

	cl_git_pass(
		git_status_foreach_ext(repo, &opts, cb_status__normal, &counts)
	);

	cl_assert_equal_i(counts.expected_entry_count, counts.entry_count);
	cl_assert_equal_i(0, counts.wrong_status_flags_count);
	cl_assert_equal_i(0, counts.wrong_sorted_path);
}

void test_status_worktree__swap_subdir_with_recurse_and_pathspec(void)
{
	status_entry_counts counts;
	git_repository *repo = cl_git_sandbox_init("status");
	git_status_options opts;

	/* first alter the contents of the worktree */
	cl_git_pass(p_rename("status/current_file", "status/swap"));
	cl_git_pass(p_rename("status/subdir", "status/current_file"));
	cl_git_pass(p_rename("status/swap", "status/subdir"));
	cl_git_mkfile("status/.new_file", "dummy");
	cl_git_pass(git_futils_mkdir_r("status/zzz_new_dir", NULL, 0777));
	cl_git_mkfile("status/zzz_new_dir/new_file", "dummy");
	cl_git_mkfile("status/zzz_new_file", "dummy");

	/* now get status */
	memset(&counts, 0x0, sizeof(status_entry_counts));
	counts.expected_entry_count = entry_count4;
	counts.expected_paths = entry_paths4;
	counts.expected_statuses = entry_statuses4;

	memset(&opts, 0, sizeof(opts));
	opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
	/* TODO: set pathspec to "current_file" eventually */

	cl_git_pass(
		git_status_foreach_ext(repo, &opts, cb_status__normal, &counts)
	);

	cl_assert_equal_i(counts.expected_entry_count, counts.entry_count);
	cl_assert_equal_i(0, counts.wrong_status_flags_count);
	cl_assert_equal_i(0, counts.wrong_sorted_path);
}

/* this test is equivalent to t18-status.c:singlestatus0 */
void test_status_worktree__single_file(void)
{
	int i;
	unsigned int status_flags;
	git_repository *repo = cl_git_sandbox_init("status");

	for (i = 0; i < (int)entry_count0; i++) {
		cl_git_pass(
			git_status_file(&status_flags, repo, entry_paths0[i])
		);
		cl_assert(entry_statuses0[i] == status_flags);
	}
}

/* this test is equivalent to t18-status.c:singlestatus1 */
void test_status_worktree__single_nonexistent_file(void)
{
	int error;
	unsigned int status_flags;
	git_repository *repo = cl_git_sandbox_init("status");

	error = git_status_file(&status_flags, repo, "nonexistent");
	cl_git_fail(error);
	cl_assert(error == GIT_ENOTFOUND);
}

/* this test is equivalent to t18-status.c:singlestatus2 */
void test_status_worktree__single_nonexistent_file_empty_repo(void)
{
	int error;
	unsigned int status_flags;
	git_repository *repo = cl_git_sandbox_init("empty_standard_repo");

	error = git_status_file(&status_flags, repo, "nonexistent");
	cl_git_fail(error);
	cl_assert(error == GIT_ENOTFOUND);
}

/* this test is equivalent to t18-status.c:singlestatus3 */
void test_status_worktree__single_file_empty_repo(void)
{
	unsigned int status_flags;
	git_repository *repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_mkfile("empty_standard_repo/new_file", "new_file\n");

	cl_git_pass(git_status_file(&status_flags, repo, "new_file"));
	cl_assert(status_flags == GIT_STATUS_WT_NEW);
}

/* this test is equivalent to t18-status.c:singlestatus4 */
void test_status_worktree__single_folder(void)
{
	int error;
	unsigned int status_flags;
	git_repository *repo = cl_git_sandbox_init("status");

	error = git_status_file(&status_flags, repo, "subdir");
	cl_git_fail(error);
	cl_assert(error != GIT_ENOTFOUND);
}


void test_status_worktree__ignores(void)
{
	int i, ignored;
	git_repository *repo = cl_git_sandbox_init("status");

	for (i = 0; i < (int)entry_count0; i++) {
		cl_git_pass(
			git_status_should_ignore(&ignored, repo, entry_paths0[i])
		);
		cl_assert(ignored == (entry_statuses0[i] == GIT_STATUS_IGNORED));
	}

	cl_git_pass(
		git_status_should_ignore(&ignored, repo, "nonexistent_file")
	);
	cl_assert(!ignored);

	cl_git_pass(
		git_status_should_ignore(&ignored, repo, "ignored_nonexistent_file")
	);
	cl_assert(ignored);
}

static int cb_status__check_592(const char *p, unsigned int s, void *payload)
{
	GIT_UNUSED(payload);

	if (s != GIT_STATUS_WT_DELETED || (payload != NULL && strcmp(p, (const char *)payload) != 0))
		return -1;

	return 0;
}

void test_status_worktree__issue_592(void)
{
	git_repository *repo;
	git_buf path = GIT_BUF_INIT;

	repo = cl_git_sandbox_init("issue_592");
	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(repo), "l.txt"));
	cl_git_pass(p_unlink(git_buf_cstr(&path)));

	cl_git_pass(git_status_foreach(repo, cb_status__check_592, "l.txt"));

	git_buf_free(&path);
}

void test_status_worktree__issue_592_2(void)
{
	git_repository *repo;
	git_buf path = GIT_BUF_INIT;

	repo = cl_git_sandbox_init("issue_592");
	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(repo), "c/a.txt"));
	cl_git_pass(p_unlink(git_buf_cstr(&path)));

	cl_git_pass(git_status_foreach(repo, cb_status__check_592, "c/a.txt"));

	git_buf_free(&path);
}

void test_status_worktree__issue_592_3(void)
{
	git_repository *repo;
	git_buf path = GIT_BUF_INIT;

	repo = cl_git_sandbox_init("issue_592");

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(repo), "c"));
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), GIT_DIRREMOVAL_FILES_AND_DIRS));

	cl_git_pass(git_status_foreach(repo, cb_status__check_592, "c/a.txt"));

	git_buf_free(&path);
}

void test_status_worktree__issue_592_4(void)
{
	git_repository *repo;
	git_buf path = GIT_BUF_INIT;

	repo = cl_git_sandbox_init("issue_592");

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(repo), "t/b.txt"));
	cl_git_pass(p_unlink(git_buf_cstr(&path)));

	cl_git_pass(git_status_foreach(repo, cb_status__check_592, "t/b.txt"));

	git_buf_free(&path);
}

void test_status_worktree__issue_592_5(void)
{
	git_repository *repo;
	git_buf path = GIT_BUF_INIT;

	repo = cl_git_sandbox_init("issue_592");

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(repo), "t"));
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), GIT_DIRREMOVAL_FILES_AND_DIRS));
	cl_git_pass(p_mkdir(git_buf_cstr(&path), 0777));

	cl_git_pass(git_status_foreach(repo, cb_status__check_592, NULL));

	git_buf_free(&path);
}

void test_status_worktree__issue_592_ignores_0(void)
{
	int count = 0;
	status_entry_single st;
	git_repository *repo = cl_git_sandbox_init("issue_592");

	cl_git_pass(git_status_foreach(repo, cb_status__count, &count));
	cl_assert_equal_i(0, count);

	cl_git_rewritefile("issue_592/.gitignore",
		".gitignore\n*.txt\nc/\n[tT]*/\n");

	cl_git_pass(git_status_foreach(repo, cb_status__count, &count));
	cl_assert_equal_i(1, count);

	/* This is a situation where the behavior of libgit2 is
	 * different from core git.  Core git will show ignored.txt
	 * in the list of ignored files, even though the directory
	 * "t" is ignored and the file is untracked because we have
	 * the explicit "*.txt" ignore rule.  Libgit2 just excludes
	 * all untracked files that are contained within ignored
	 * directories without explicitly listing them.
	 */
	cl_git_rewritefile("issue_592/t/ignored.txt", "ping");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(repo, cb_status__single, &st));
	cl_assert_equal_i(1, st.count);
	cl_assert(st.status == GIT_STATUS_IGNORED);

	cl_git_rewritefile("issue_592/c/ignored_by_dir", "ping");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(repo, cb_status__single, &st));
	cl_assert_equal_i(1, st.count);
	cl_assert(st.status == GIT_STATUS_IGNORED);

	cl_git_rewritefile("issue_592/t/ignored_by_dir_pattern", "ping");

	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(repo, cb_status__single, &st));
	cl_assert_equal_i(1, st.count);
	cl_assert(st.status == GIT_STATUS_IGNORED);
}

void test_status_worktree__issue_592_ignored_dirs_with_tracked_content(void)
{
	int count = 0;
	git_repository *repo = cl_git_sandbox_init("issue_592b");

	cl_git_pass(git_status_foreach(repo, cb_status__count, &count));
	cl_assert_equal_i(1, count);

	/* if we are really mimicking core git, then only ignored1.txt
	 * at the top level will show up in the ignores list here.
	 * everything else will be unmodified or skipped completely.
	 */
}

void test_status_worktree__cannot_retrieve_the_status_of_a_bare_repository(void)
{
	git_repository *repo;
	int error;
	unsigned int status = 0;

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));

	error = git_status_file(&status, repo, "dummy");

	cl_git_fail(error);
	cl_assert(error != GIT_ENOTFOUND);

	git_repository_free(repo);
}

void test_status_worktree__first_commit_in_progress(void)
{
	git_repository *repo;
	git_index *index;
	status_entry_single result;

	cl_git_pass(git_repository_init(&repo, "getting_started", 0));
	cl_git_mkfile("getting_started/testfile.txt", "content\n");

	memset(&result, 0, sizeof(result));
	cl_git_pass(git_status_foreach(repo, cb_status__single, &result));
	cl_assert_equal_i(1, result.count);
	cl_assert(result.status == GIT_STATUS_WT_NEW);

	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_index_add(index, "testfile.txt", 0));
	cl_git_pass(git_index_write(index));

	memset(&result, 0, sizeof(result));
	cl_git_pass(git_status_foreach(repo, cb_status__single, &result));
	cl_assert_equal_i(1, result.count);
	cl_assert(result.status == GIT_STATUS_INDEX_NEW);

	git_index_free(index);
	git_repository_free(repo);
}



void test_status_worktree__status_file_without_index_or_workdir(void)
{
	git_repository *repo;
	unsigned int status = 0;
	git_index *index;

	cl_git_pass(p_mkdir("wd", 0777));

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
	cl_git_pass(git_repository_set_workdir(repo, "wd"));

	cl_git_pass(git_index_open(&index, "empty-index"));
	cl_assert_equal_i(0, git_index_entrycount(index));
	git_repository_set_index(repo, index);

	cl_git_pass(git_status_file(&status, repo, "branch_file.txt"));

	cl_assert_equal_i(GIT_STATUS_INDEX_DELETED, status);

	git_repository_free(repo);
	git_index_free(index);
	cl_git_pass(p_rmdir("wd"));
}

static void fill_index_wth_head_entries(git_repository *repo, git_index *index)
{
	git_oid oid;
	git_commit *commit;
	git_tree *tree;

	cl_git_pass(git_reference_name_to_oid(&oid, repo, "HEAD"));
	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_commit_tree(&tree, commit));

	cl_git_pass(git_index_read_tree(index, tree));
	cl_git_pass(git_index_write(index));

	git_tree_free(tree);
	git_commit_free(commit);
}

void test_status_worktree__status_file_with_clean_index_and_empty_workdir(void)
{
	git_repository *repo;
	unsigned int status = 0;
	git_index *index;

	cl_git_pass(p_mkdir("wd", 0777));

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
	cl_git_pass(git_repository_set_workdir(repo, "wd"));

	cl_git_pass(git_index_open(&index, "my-index"));
	fill_index_wth_head_entries(repo, index);

	git_repository_set_index(repo, index);

	cl_git_pass(git_status_file(&status, repo, "branch_file.txt"));

	cl_assert_equal_i(GIT_STATUS_WT_DELETED, status);

	git_repository_free(repo);
	git_index_free(index);
	cl_git_pass(p_rmdir("wd"));
	cl_git_pass(p_unlink("my-index"));
}
