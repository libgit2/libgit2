#include "clar_libgit2.h"
#include "fileops.h"
#include "ignore.h"
#include "status_data.h"


/**
 * Test fixtures
 */
static git_repository *_repository = NULL;


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
	return GIT_SUCCESS;
}

static int
cb_status__count(const char *GIT_UNUSED(p), unsigned int GIT_UNUSED(s), void *payload)
{
	volatile int *count = (int *)payload;

	GIT_UNUSED_ARG(p);
	GIT_UNUSED_ARG(s);

	*count++;

	return GIT_SUCCESS;
}



/**
 * Initializer
 *
 * This method is called once before starting each
 * test, and will load the required fixtures
 */
void test_status_worktree__initialize(void)
{
	/*
	 * Sandbox the `status/` repository from our Fixtures.
	 * This will copy the whole folder to our sandbox,
	 * so now it can be accessed with `./status`
	 */
	cl_fixture_sandbox("status");

	/*
	 * Rename `status/.gitted` to `status/.git`
	 * We do this because we cannot store a folder named `.git`
	 * inside the fixtures folder in our libgit2 repo.
	 */
	cl_git_pass(
		p_rename("status/.gitted", "status/.git")
	);

	/*
	 * Open the sandboxed "status" repository
	 */
	cl_git_pass(git_repository_open(&_repository, "status/.git"));
}

/**
 * Cleanup
 *
 * This will be called once after each test finishes, even
 * if the test failed
 */
void test_status_worktree__cleanup(void)
{
	git_repository_free(_repository);
	_repository = NULL;

	cl_fixture_cleanup("status");
}

/**
 * Tests - Status determination on a working tree
 */
void test_status_worktree__whole_repository(void)
{
	struct status_entry_counts counts;

	memset(&counts, 0x0, sizeof(struct status_entry_counts));
	counts.expected_entry_count = entry_count0;
	counts.expected_paths = entry_paths0;
	counts.expected_statuses = entry_statuses0;

	cl_git_pass(
		git_status_foreach(_repository, cb_status__normal, &counts)
	);

	cl_assert(counts.entry_count == counts.expected_entry_count);
	cl_assert(counts.wrong_status_flags_count == 0);
	cl_assert(counts.wrong_sorted_path == 0);
}

void test_status_worktree__empty_repository(void)
{
	int count = 0;

	git_status_foreach(_repository, cb_status__count, &count);
	cl_assert(count == 0);
}

void test_status_worktree__single_file(void)
{
	int i;
	unsigned int status_flags;

	for (i = 0; i < (int)entry_count0; i++) {
		cl_git_pass(
			git_status_file(&status_flags, _repository, entry_paths0[i])
		);
		cl_assert(entry_statuses0[i] == status_flags);
	}
}

void test_status_worktree__ignores(void)
{
	int i, ignored;

	for (i = 0; i < (int)entry_count0; i++) {
		cl_git_pass(git_status_should_ignore(_repository, entry_paths0[i], &ignored));
		cl_assert(ignored == (entry_statuses0[i] == GIT_STATUS_IGNORED));
	}

	cl_git_pass(git_status_should_ignore(_repository, "nonexistent_file", &ignored));
	cl_assert(!ignored);

	cl_git_pass(git_status_should_ignore(_repository, "ignored_nonexistent_file", &ignored));
	cl_assert(ignored);
}
