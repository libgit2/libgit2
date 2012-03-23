#include "clar_libgit2.h"
#include "fileops.h"
#include "ignore.h"
#include "status_data.h"
#include "posix.h"
#include "util.h"
#include "path.h"

/**
 * Auxiliary methods
 */
static int
cb_status__normal( const char *path, unsigned int status_flags, void *payload)
{
	struct status_entry_counts *counts = payload;

	if (counts->entry_count >= counts->expected_entry_count) {
		counts->wrong_status_flags_count++;
		goto exit;
	}

	if (strcmp(path, counts->expected_paths[counts->entry_count])) {
		counts->wrong_sorted_path++;
		goto exit;
	}

	if (status_flags != counts->expected_statuses[counts->entry_count])
		counts->wrong_status_flags_count++;

exit:
	counts->entry_count++;
	return 0;
}

static int
cb_status__count(const char *p, unsigned int s, void *payload)
{
	volatile int *count = (int *)payload;

	GIT_UNUSED(p);
	GIT_UNUSED(s);

	(*count)++;

	return 0;
}

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
	struct status_entry_counts counts;
	git_repository *repo = cl_git_sandbox_init("status");

	memset(&counts, 0x0, sizeof(struct status_entry_counts));
	counts.expected_entry_count = entry_count0;
	counts.expected_paths = entry_paths0;
	counts.expected_statuses = entry_statuses0;

	cl_git_pass(
		git_status_foreach(repo, cb_status__normal, &counts)
	);

	cl_assert(counts.entry_count == counts.expected_entry_count);
	cl_assert(counts.wrong_status_flags_count == 0);
	cl_assert(counts.wrong_sorted_path == 0);
}

/* this test is equivalent to t18-status.c:statuscb1 */
void test_status_worktree__empty_repository(void)
{
	int count = 0;
	git_repository *repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_status_foreach(repo, cb_status__count, &count));

	cl_assert(count == 0);
}

static int remove_file_cb(void *data, git_buf *file)
{
	const char *filename = git_buf_cstr(file);

	GIT_UNUSED(data);

	if (git__suffixcmp(filename, ".git") == 0)
		return 0;

	if (git_path_isdir(filename))
		cl_git_pass(git_futils_rmdir_r(filename, 1));
	else
		cl_git_pass(p_unlink(git_buf_cstr(file)));

	return 0;
}

/* this test is equivalent to t18-status.c:statuscb2 */
void test_status_worktree__purged_worktree(void)
{
	struct status_entry_counts counts;
	git_repository *repo = cl_git_sandbox_init("status");
	git_buf workdir = GIT_BUF_INIT;

	/* first purge the contents of the worktree */
	cl_git_pass(git_buf_sets(&workdir, git_repository_workdir(repo)));
	cl_git_pass(git_path_direach(&workdir, remove_file_cb, NULL));

	/* now get status */
	memset(&counts, 0x0, sizeof(struct status_entry_counts));
	counts.expected_entry_count = entry_count2;
	counts.expected_paths = entry_paths2;
	counts.expected_statuses = entry_statuses2;

	cl_git_pass(
		git_status_foreach(repo, cb_status__normal, &counts)
	);

	cl_assert(counts.entry_count == counts.expected_entry_count);
	cl_assert(counts.wrong_status_flags_count == 0);
	cl_assert(counts.wrong_sorted_path == 0);
}

/* this test is equivalent to t18-status.c:statuscb3 */
void test_status_worktree__swap_subdir_and_file(void)
{
	struct status_entry_counts counts;
	git_repository *repo = cl_git_sandbox_init("status");

	/* first alter the contents of the worktree */
	cl_git_pass(p_rename("status/current_file", "status/swap"));
	cl_git_pass(p_rename("status/subdir", "status/current_file"));
	cl_git_pass(p_rename("status/swap", "status/subdir"));

	cl_git_mkfile("status/.HEADER", "dummy");
	cl_git_mkfile("status/42-is-not-prime.sigh", "dummy");
	cl_git_mkfile("status/README.md", "dummy");

	/* now get status */
	memset(&counts, 0x0, sizeof(struct status_entry_counts));
	counts.expected_entry_count = entry_count3;
	counts.expected_paths = entry_paths3;
	counts.expected_statuses = entry_statuses3;

	cl_git_pass(
		git_status_foreach(repo, cb_status__normal, &counts)
	);

	cl_assert(counts.entry_count == counts.expected_entry_count);
	cl_assert(counts.wrong_status_flags_count == 0);
	cl_assert(counts.wrong_sorted_path == 0);

}

void test_status_worktree__swap_subdir_with_recurse_and_pathspec(void)
{
	struct status_entry_counts counts;
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
	memset(&counts, 0x0, sizeof(struct status_entry_counts));
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

	cl_assert(counts.entry_count == counts.expected_entry_count);
	cl_assert(counts.wrong_status_flags_count == 0);
	cl_assert(counts.wrong_sorted_path == 0);
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
}


void test_status_worktree__ignores(void)
{
	int i, ignored;
	git_repository *repo = cl_git_sandbox_init("status");

	for (i = 0; i < (int)entry_count0; i++) {
		cl_git_pass(
			git_status_should_ignore(repo, entry_paths0[i], &ignored)
		);
		cl_assert(ignored == (entry_statuses0[i] == GIT_STATUS_IGNORED));
	}

	cl_git_pass(
		git_status_should_ignore(repo, "nonexistent_file", &ignored)
	);
	cl_assert(!ignored);

	cl_git_pass(
		git_status_should_ignore(repo, "ignored_nonexistent_file", &ignored)
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
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), 1));

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
	cl_git_pass(git_futils_rmdir_r(git_buf_cstr(&path), 1));
	cl_git_pass(p_mkdir(git_buf_cstr(&path), 0777));

	cl_git_pass(git_status_foreach(repo, cb_status__check_592, NULL));

	git_buf_free(&path);
}
