#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "merge.h"
#include "../merge_helpers.h"
#include "../conflict_data.h"
#include "refs.h"
#include "futils.h"

static git_repository *repo;
static git_index *repo_index;

#define TEST_REPO_PATH "merge-octopus"
#define TEST_INDEX_PATH TEST_REPO_PATH "/.git/index"

#define THEIRS_SIMPLE_BRANCHES		{"branch1", "branch1-b", "branch2", "branch3"}
#define THEIRS_SIMPLE_OIDS			{"ab409242f7ade0f4823a51fa0a87a553cafd1c14", \
                                     "1ae815cbcbf1cc17bb338bf584de73bd9241ee84", \
                                     "53a65eaa4ff1167f183e2dd91b6cde4d9d3200ce", \
                                     "e6678029e2f92db6539e161cf43d467aff87157f"}

#define OUR_TARGET_BRANCH "t"
#define NUM_COMMITS 4


#define BRANCH_T_INDEX_ENTRY_1 \
        { 0100644, "2c6e75856f24a210d21973441874bb1512fc5edd", 0, \
          "1.txt" }

#define BRANCH1_B_INDEX_ENTRY \
        { 0100644, "374cd34a4752890de19d6c8a39510dbbe69406a5", 0, \
          "added-in-branch1.txt" }

#define BRANCH2_INDEX_ENTRY \
        { 0100644, "527a828f16ab91bc9004269d2e3e3f30cbc8a854", 0, \
          "added-in-branch2.txt" }

#define BRANCH3_INDEX_ENTRY \
        { 0100644, "dc61ca12000e7be2c4ea40398e783086428d053e", 0, \
          "added-in-branch3.txt" }

#define MASTER_INDEX_ENTRY \
        { 0100644, "1f7391f92b6a3792204e07e99f71f643cc35e7e1", 0, \
          "master.txt" }

#define BRANCH_T_INDEX_ENTRY_T \
        { 0100644, "4b4d41231929d23b3f1de89c00b339831ebaa2b4", 0, \
          "t.txt" }

#define BRANCH_T_INDEX_ENTRY_T1 \
        { 0100644, "f15c1d17d1ebef8adbdb7379cc3dcf7f48fa9cb5", 0, \
          "t1.txt" }

#define EXPECTED_INDEX_ENTRY_COUNT 7

/* Fixture setup and teardown */
void test_merge_octopus_simple__initialize(void)
{
	git_config *cfg;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);

	/* Ensure that the user's merge.conflictstyle doesn't interfere */
	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_string(cfg, "merge.conflictstyle", "merge"));
	git_config_free(cfg);
}

void test_merge_octopus_simple__cleanup(void)
{
	git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

static void octopus_merge(int merge_file_favor, int addl_checkout_strategy)
{
    int i;
    char* oid_strings[NUM_COMMITS] = THEIRS_SIMPLE_OIDS;
	git_oid their_oids[NUM_COMMITS];
	git_annotated_commit *their_heads[NUM_COMMITS];
	git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

    for(i = 0; i < NUM_COMMITS; ++i) {
        cl_git_pass(git_oid_from_string(&their_oids[i], oid_strings[i], GIT_OID_SHA1));
        cl_git_pass(git_annotated_commit_lookup(&their_heads[i], repo, &their_oids[i]));
    }

	merge_opts.file_favor = merge_file_favor;
	checkout_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS |
		addl_checkout_strategy;

	cl_git_pass(git_merge(repo, (const git_annotated_commit **)their_heads, NUM_COMMITS, &merge_opts, &checkout_opts));

    for(i = 0; i < NUM_COMMITS; ++i) {
        git_annotated_commit_free(their_heads[i]);
    }
}

void test_merge_octopus_ffskip__fastforward_and_skip(void)
{
    struct merge_index_entry merge_index_entries[] = {
		BRANCH_T_INDEX_ENTRY_1,
        BRANCH1_B_INDEX_ENTRY,
        BRANCH2_INDEX_ENTRY,
        BRANCH3_INDEX_ENTRY,
        MASTER_INDEX_ENTRY,
		BRANCH_T_INDEX_ENTRY_T,
		BRANCH_T_INDEX_ENTRY_T1
    };

	octopus_merge(0, 0);

    cl_assert(merge_test_index(repo_index, merge_index_entries, EXPECTED_INDEX_ENTRY_COUNT));
}

