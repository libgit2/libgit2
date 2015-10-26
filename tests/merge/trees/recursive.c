#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "merge.h"
#include "../merge_helpers.h"

static git_repository *repo;

#define TEST_REPO_PATH "merge-recursive"

void test_merge_trees_recursive__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
}

void test_merge_trees_recursive__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_merge_trees_recursive__one(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "dea7215f259b2cced87d1bda6c72f8b4ce37a2ff", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "94d2c01087f48213bd157222d54edfefd77c9bba", 0, "veal.txt" },
	};

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchA-1", "branchA-2", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 6));

	git_index_free(index);
}

void test_merge_trees_recursive__one_norecursive(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "dea7215f259b2cced87d1bda6c72f8b4ce37a2ff", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "94d2c01087f48213bd157222d54edfefd77c9bba", 0, "veal.txt" },
	};

	opts.flags |= GIT_MERGE_NO_RECURSIVE;

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchA-1", "branchA-2", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 6));

	git_index_free(index);
}

void test_merge_trees_recursive__two(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "666ffdfcf1eaa5641fa31064bf2607327e843c09", 0, "veal.txt" },
	};

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchB-1", "branchB-2", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 6));

	git_index_free(index);
}

void test_merge_trees_recursive__two_norecursive(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "cb49ad76147f5f9439cbd6133708b76142660660", 1, "veal.txt" },
		{ 0100644, "b2a81ead9e722af0099fccfb478cea88eea749a2", 2, "veal.txt" },
		{ 0100644, "4e21d2d63357bde5027d1625f5ec6b430cdeb143", 3, "veal.txt" },
	};

	opts.flags |= GIT_MERGE_NO_RECURSIVE;

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchB-1", "branchB-2", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 8));

	git_index_free(index);
}

void test_merge_trees_recursive__three(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "15faa0c9991f2d65686e844651faa2ff9827887b", 0, "veal.txt" },
	};

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchC-1", "branchC-2", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 6));

	git_index_free(index);
}

void test_merge_trees_recursive__three_norecursive(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "b2a81ead9e722af0099fccfb478cea88eea749a2", 1, "veal.txt" },
		{ 0100644, "898d12687fb35be271c27c795a6b32c8b51da79e", 2, "veal.txt" },
		{ 0100644, "68a2e1ee61a23a4728fe6b35580fbbbf729df370", 3, "veal.txt" },
	};

	opts.flags |= GIT_MERGE_NO_RECURSIVE;

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchC-1", "branchC-2", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 8));

	git_index_free(index);
}

void test_merge_trees_recursive__four(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "d55e5dc038c52f1a36548625bcb666cbc06db9e6", 0, "veal.txt" },
	};

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchD-2", "branchD-1", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 6));

	git_index_free(index);
}

void test_merge_trees_recursive__four_norecursive(void)
{
	git_index *index;
	git_merge_options opts = GIT_MERGE_OPTIONS_INIT;

	struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "ffb36e513f5fdf8a6ba850a20142676a2ac4807d", 0, "asparagus.txt" },
		{ 0100644, "68f6182f4c85d39e1309d97c7e456156dc9c0096", 0, "beef.txt" },
		{ 0100644, "4b7c5650008b2e747fe1809eeb5a1dde0e80850a", 0, "bouilli.txt" },
		{ 0100644, "c4e6cca3ec6ae0148ed231f97257df8c311e015f", 0, "gravy.txt" },
		{ 0100644, "68af1fc7407fd9addf1701a87eb1c95c7494c598", 0, "oyster.txt" },
		{ 0100644, "898d12687fb35be271c27c795a6b32c8b51da79e", 1, "veal.txt" },
		{ 0100644, "f1b44c04989a3a1c14b036cfadfa328d53a7bc5e", 2, "veal.txt" },
		{ 0100644, "5e8747f5200fac0f945a07daf6163ca9cb1a8da9", 3, "veal.txt" },
	};

	opts.flags |= GIT_MERGE_NO_RECURSIVE;

	cl_git_pass(merge_commits_from_branches(&index, repo, "branchD-2", "branchD-1", &opts));

	cl_assert(merge_test_index(index, merge_index_entries, 8));

	git_index_free(index);
}

