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

/*
 * Relevant branch graph of the situation before the octopus merge.
 *		*   1b7ad0f (2) Merge branch 't2' into 2
 *		|\
 *		| * 96e6022 (t2) add in t2
 *		* | 80194ba append in 2 again
 *		| | * a49a6a4 (2a) add in 2a
 *		| |/
 *		|/|
 *		* | 7b07a0f append in 2
 *		* | 0d37365 add in 2
 *		| | * 026a849 (t1) append in t1 again
 *		| |/
 *		| | * 6d7a094 (1b) append in 1b
 *		| | | *   5136b71 (1) Merge branch '1b' into 1
 *		| | | |\
 *		| | | |/
 *		| | |/|
 *		| | * | 862ab60 add in 1b
 *		| | | * 2686687 append in 1 again
 *		| | |/
 *		| | | *   ec7080d (HEAD -> t) Merge branch '1' into t
 *		| | | |\
 *		| | | |/
 *		| | |/|
 *		| | * | 3e25ef4 (skippable) append in 1
 *		| | | * 4d098df append again in t
 *		| | | *   1701254 Merge branch 't1' into t
 *		| | | |\
 *		| | |_|/
 *		| |/| |
 *		| * | | f5683f6 append in t1
 *		| * | | b82129c add in t1
 *		| | | * 5868f5a append in t
 *		| | |/
 *		| |/|
 *		| * | bb06048 add in t
 *		|/ /
 *		| | * e2161c7 (1a) append in 1a
 *		| | * 3bbd2a3 add in 1a
 *		| |/
 *		| * cf9ed0f add in 1
 *		|/
 *		| * 75f1c45 (3) add in 3
 *		|/
 *		* 91a7496 (unskippable, master) add in master
 */

#define TEST_REPO_PATH "merge-octopus"
#define TEST_INDEX_PATH TEST_REPO_PATH "/.git/index"

#define THEIRS_SIMPLE_BRANCHES		{"1", "1a", "1b", "2", "2a", "3", "t1", "t2"}
#define THEIRS_SIMPLE_OIDS			{"5136b71930b78146dfbe5f4c080c54e05b1f884a", \
									 "e2161c7b0ef124afe04c553fbd6f8e8156b947f5", \
									 "6d7a0948633012aa9038274538f76c968497b2ea", \
									 "1b7ad0f7343ff9ce03703cc40cf775b0e9cc57fe", \
									 "a49a6a4c527b223a1bdc1ae45e1260e1d041bf12", \
									 "75f1c450c1196e953e1dbfab827765a19623c856", \
									 "026a849d3c17944b00bd2de4840591df30852769", \
                                     "96e602252e180110495f303b6164a7d1158de595"}

#define OUR_TARGET_BRANCH "t" 
#define NUM_COMMITS 8

#define INDEX_ENTRY_1_TXT \
        { 0100644, "e7c1228a15149b7459531590842ff5e610e1a5c5", 0, \
          "1.txt" }

#define INDEX_ENTRY_1A_TXT \
        { 0100644, "5ba15720d00755ff42ae0b7a3628c08326958ca7", 0, \
          "1a.txt" }

#define INDEX_ENTRY_1B_TXT \
        { 0100644, "2481a2cc662ce05a7f0e52bd283403654a24d61c", 0, \
          "1b.txt" }

#define INDEX_ENTRY_2_TXT \
        { 0100644, "b2b120f3b488e6f80674f6b5c89aaec152485c66", 0, \
          "2.txt" }

#define INDEX_ENTRY_2A_TXT \
        { 0100644, "09f4002ed8b3d379ac0f9322f9679c1006172bc3", 0, \
          "2a.txt" }

#define INDEX_ENTRY_3_TXT \
        { 0100644, "88a56d9ad6353e551de6d5025348e413b1c5d13f", 0, \
          "3.txt" }

#define INDEX_ENTRY_MASTER_TXT \
        { 0100644, "1f7391f92b6a3792204e07e99f71f643cc35e7e1", 0, \
          "master.txt" }

#define INDEX_ENTRY_T_TXT \
        { 0100644, "4b4d41231929d23b3f1de89c00b339831ebaa2b4", 0, \
          "t.txt" }

#define INDEX_ENTRY_T1_TXT \
        { 0100644, "ccfe29d825cb6476a3b1bf27d68a3edd4fd86c0b", 0, \
          "t1.txt" }

#define INDEX_ENTRY_T2_TXT \
        { 0100644, "5fe609a987f5e38b5145ca136d5a0768629da47c", 0, \
          "t2.txt" }

/* Fixture setup and teardown */
void test_merge_octopus_complex__initialize(void)
{
	git_config *cfg;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);

	/* Ensure that the user's merge.conflictstyle doesn't interfere */
	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_string(cfg, "merge.conflictstyle", "merge"));
	git_config_free(cfg);
}

void test_merge_octopus_complex__cleanup(void)
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

void test_merge_octopus_complex__merge_multiple_commits(void)
{
    struct merge_index_entry merge_index_entries[] = {
        INDEX_ENTRY_1_TXT,
        INDEX_ENTRY_1A_TXT,
        INDEX_ENTRY_1B_TXT,
        INDEX_ENTRY_2_TXT,
        INDEX_ENTRY_2A_TXT,
        INDEX_ENTRY_3_TXT,
        INDEX_ENTRY_MASTER_TXT,
        INDEX_ENTRY_T_TXT,
        INDEX_ENTRY_T1_TXT,
        INDEX_ENTRY_T2_TXT
    };

	octopus_merge(0, 0);

    cl_assert(merge_test_index(repo_index, merge_index_entries, 10));
}

