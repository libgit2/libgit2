#include "clar_libgit2.h"
#include "fileops.h"
#include "ignore.h"
#include "status_data.h"
#include "posix.h"

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

void test_status_worktree__empty_repository(void)
{
	int count = 0;
	git_repository *repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_pass(git_status_foreach(repo, cb_status__count, &count));

	cl_assert(count == 0);
}

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
