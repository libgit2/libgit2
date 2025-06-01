#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "merge.h"
#include "../merge_helpers.h"

static git_repository *repo;

#define TEST_REPO_PATH "merge-resolve"

#define SUBMODULE_MAIN_BRANCH		"submodules"
#define SUBMODULE_OTHER_BRANCH		"submodules-branch"
#define SUBMODULE_OTHER2_BRANCH		"submodules-branch2"
#define SUBMODULE_DELETE_BRANCH		"delete-submodule"

#define TEST_INDEX_PATH TEST_REPO_PATH "/.git/index"

/* Fixture setup and teardown */
void test_merge_workdir_submodules__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
}

void test_merge_workdir_submodules__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_merge_workdir_submodules__automerge(void)
{
	git_reference *our_ref, *their_ref;
	git_commit *our_commit;
	git_annotated_commit *their_head;
	git_index *index;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "caff6b7d44973f53e3e0cf31d0d695188b19aec6", 0, ".gitmodules" },
		{ 0100644, "950a663a6a7b2609eed1ed1ba9f41eb1a3192a9f", 0, "file1.txt" },
		{ 0100644, "343e660b9cb4bee5f407c2e33fcb9df24d9407a4", 0, "file2.txt" },
		{ 0160000, "d3d806a4bef96889117fd7ebac0e3cb5ec152932", 1, "submodule" },
		{ 0160000, "297aa6cd028b3336c7802c7a6f49143da4e1602d", 2, "submodule" },
		{ 0160000, "ae39c77c70cb6bad18bb471912460c4e1ba0f586", 3, "submodule" },
	};

	cl_git_pass(git_reference_lookup(&our_ref, repo, "refs/heads/" SUBMODULE_MAIN_BRANCH));
	cl_git_pass(git_commit_lookup(&our_commit, repo, git_reference_target(our_ref)));
	cl_git_pass(git_reset(repo, (git_object *)our_commit, GIT_RESET_HARD, NULL));

	cl_git_pass(git_reference_lookup(&their_ref, repo, "refs/heads/" SUBMODULE_OTHER_BRANCH));
	cl_git_pass(git_annotated_commit_from_ref(&their_head, repo, their_ref));

	cl_git_pass(git_merge(repo, (const git_annotated_commit **)&their_head, 1, NULL, NULL));

	cl_git_pass(git_repository_index(&index, repo));
	cl_assert(merge_test_index(index, merge_index_entries, 6));
	cl_assert_equal_i(true, git_index_has_conflicts(index));

	/* Put an actual Git repository into the submodule path on disk.
	 * Add it to the index and assert that the conflict is resolved.
	 */
	cl_fixture_sandbox("testrepo");
	p_rename("testrepo", TEST_REPO_PATH "/submodule");
	p_rename(TEST_REPO_PATH "/submodule/.gitted", TEST_REPO_PATH "/submodule/.git");
	cl_git_pass(git_index_add_bypath(index, "submodule"));
	cl_assert_equal_i(false, git_index_has_conflicts(index));

	git_index_free(index);
	git_annotated_commit_free(their_head);
	git_commit_free(our_commit);
	git_reference_free(their_ref);
	git_reference_free(our_ref);
}

void test_merge_workdir_submodules__take_changed(void)
{
	git_reference *our_ref, *their_ref;
	git_commit *our_commit;
	git_annotated_commit *their_head;
	git_index *index;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "caff6b7d44973f53e3e0cf31d0d695188b19aec6", 0, ".gitmodules" },
		{ 0100644, "b438ff23300b2e0f80b84a6f30140dfa91e71423", 0, "file1.txt" },
		{ 0100644, "f27fbafdfa6693f8f7a5128506fe3e338dbfcad2", 0, "file2.txt" },
		{ 0160000, "297aa6cd028b3336c7802c7a6f49143da4e1602d", 0, "submodule" },
	};

	cl_git_pass(git_reference_lookup(&our_ref, repo, "refs/heads/" SUBMODULE_MAIN_BRANCH));
	cl_git_pass(git_commit_lookup(&our_commit, repo, git_reference_target(our_ref)));
	cl_git_pass(git_reset(repo, (git_object *)our_commit, GIT_RESET_HARD, NULL));

	cl_git_pass(git_reference_lookup(&their_ref, repo, "refs/heads/" SUBMODULE_OTHER2_BRANCH));
	cl_git_pass(git_annotated_commit_from_ref(&their_head, repo, their_ref));

	cl_git_pass(git_merge(repo, (const git_annotated_commit **)&their_head, 1, NULL, NULL));

	cl_git_pass(git_repository_index(&index, repo));
	cl_assert(merge_test_index(index, merge_index_entries, 4));

	git_index_free(index);
	git_annotated_commit_free(their_head);
	git_commit_free(our_commit);
	git_reference_free(their_ref);
	git_reference_free(our_ref);
}


void test_merge_workdir_submodules__update_delete_conflict(void)
{
	git_reference *our_ref, *their_ref;
	git_commit *our_commit;
	git_annotated_commit *their_head;
	git_index *index;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391", 0, ".gitmodules" },
		{ 0100644, "5887a5e516c53bd58efb0f02ec6aa031b6fe9ad7", 0, "file1.txt" },
		{ 0100644, "4218670ab81cc219a9f94befb5c5dad90ec52648", 0, "file2.txt" },
		{ 0160000, "d3d806a4bef96889117fd7ebac0e3cb5ec152932", 1, "submodule"},
		{ 0160000, "297aa6cd028b3336c7802c7a6f49143da4e1602d", 3, "submodule" },
	};

	cl_git_pass(git_reference_lookup(&our_ref, repo, "refs/heads/" SUBMODULE_DELETE_BRANCH));
	cl_git_pass(git_commit_lookup(&our_commit, repo, git_reference_target(our_ref)));
	cl_git_pass(git_reset(repo, (git_object *)our_commit, GIT_RESET_HARD, NULL));

	cl_git_pass(git_reference_lookup(&their_ref, repo, "refs/heads/" SUBMODULE_MAIN_BRANCH));
	cl_git_pass(git_annotated_commit_from_ref(&their_head, repo, their_ref));

	cl_git_pass(git_merge(repo, (const git_annotated_commit **)&their_head, 1, NULL, NULL));

	cl_git_pass(git_repository_index(&index, repo));
	cl_assert(merge_test_index(index, merge_index_entries, 5));

	git_index_free(index);
	git_annotated_commit_free(their_head);
	git_commit_free(our_commit);
	git_reference_free(their_ref);
	git_reference_free(our_ref);
}
