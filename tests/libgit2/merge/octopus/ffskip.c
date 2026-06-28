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

#define THEIRS_SIMPLE_BRANCHES		{"ff", "skippable", "1"}
#define THEIRS_SIMPLE_OIDS			{"6c9e78d45edf0797f6b5af9a3e3241230c8ce65a", \
                                     "3e25ef4341e1ba0013f2fa65a9bf7298923180c5", \
									 "5136b71930b78146dfbe5f4c080c54e05b1f884a"}

#define OUR_TARGET_BRANCH "t"
#define NUM_COMMITS 3


#define INDEX_ENTRY_1_TXT \
        { 0100644, "e7c1228a15149b7459531590842ff5e610e1a5c5", 0, \
          "1.txt" }

#define INDEX_ENTRY_1B_TXT \
        { 0100644, "67e43930989305efbe75ac598126259707078305", 0, \
          "1b.txt" }

#define INDEX_ENTRY_FF_TXT \
        { 0100644, "3852122c84437abfb91b03df90677d3f1e3dbcd6", 0, \
          "ff.txt" }

#define INDEX_ENTRY_MASTER_TXT \
        { 0100644, "1f7391f92b6a3792204e07e99f71f643cc35e7e1", 0, \
          "master.txt" }

#define INDEX_ENTRY_T_TXT \
        { 0100644, "4b4d41231929d23b3f1de89c00b339831ebaa2b4", 0, \
          "t.txt" }

#define INDEX_ENTRY_T1_TXT \
        { 0100644, "f15c1d17d1ebef8adbdb7379cc3dcf7f48fa9cb5", 0, \
          "t1.txt" }

#define EXPECTED_INDEX_ENTRY_COUNT 6

/* Fixture setup and teardown */
void test_merge_octopus_ffskip__initialize(void)
{
	git_config *cfg;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);

	/* Ensure that the user's merge.conflictstyle doesn't interfere */
	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_string(cfg, "merge.conflictstyle", "merge"));
	git_config_free(cfg);
}

void test_merge_octopus_ffskip__cleanup(void)
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
		INDEX_ENTRY_1_TXT,
		INDEX_ENTRY_1B_TXT,
		INDEX_ENTRY_FF_TXT,
		INDEX_ENTRY_MASTER_TXT,
		INDEX_ENTRY_T_TXT,
		INDEX_ENTRY_T1_TXT
    };

	octopus_merge(0, 0);

    cl_assert(merge_test_index(repo_index, merge_index_entries, EXPECTED_INDEX_ENTRY_COUNT));
}

