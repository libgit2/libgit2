#include "clar_libgit2.h"
#include "apply_helpers.h"

static git_repository *repo;

#define TEST_REPO_PATH "merge-recursive"

void test_apply_callbacks__initialize(void)
{
	git_oid oid;
	git_commit *commit;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);

	git_oid_fromstr(&oid, "539bd011c4822c560c1d17cab095006b7a10f707");
	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_reset(repo, (git_object *)commit, GIT_RESET_HARD, NULL));
	git_commit_free(commit);
}

void test_apply_callbacks__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static int delta_abort_cb(const git_diff_delta *delta, void *payload)
{
	GIT_UNUSED(payload);

	if (!strcmp(delta->old_file.path, "veal.txt"))
		return -99;

	return 0;
}

void test_apply_callbacks__delta_aborts(void)
{
	git_diff *diff;
	git_apply_options opts = GIT_APPLY_OPTIONS_INIT;

	opts.delta_cb = delta_abort_cb;

	cl_git_pass(git_diff_from_buffer(&diff,
		DIFF_MODIFY_TWO_FILES, strlen(DIFF_MODIFY_TWO_FILES)));
	cl_git_fail_with(-99,
		git_apply(repo, diff, GIT_APPLY_LOCATION_INDEX, &opts));

	validate_index_unchanged(repo);
	validate_workdir_unchanged(repo);

	git_diff_free(diff);
}
