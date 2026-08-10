#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "merge.h"
#include "merge_helpers.h"
#include "conflict_data.h"
#include "refs.h"
#include "futils.h"

#define TEST_REPO_PATH "merge-octopus"

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

/* Fixture setup and teardown */
void test_merge_octopus__initialize(void)
{
	git_config *cfg;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);

	/* Ensure that the user's merge.conflictstyle doesn't interfere */
	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_string(cfg, "merge.conflictstyle", "merge"));
	git_config_free(cfg);
}

void test_merge_octopus__cleanup(void)
{
	git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

#define SIMPLE_BRANCHES { \
	"ab409242f7ade0f4823a51fa0a87a553cafd1c14", /* branch1 */   \
	"1ae815cbcbf1cc17bb338bf584de73bd9241ee84", /* branch1-b */ \
	"53a65eaa4ff1167f183e2dd91b6cde4d9d3200ce", /* branch2 */   \
	"e6678029e2f92db6539e161cf43d467aff87157f"  /* branch3 */   \
}
#define SIMPLE_BRANCH_COUNT 4

void test_merge_octopus__simple(void)
{
	char *oid_strings[SIMPLE_BRANCH_COUNT] = SIMPLE_BRANCHES;
	git_oid their_oids[SIMPLE_BRANCH_COUNT];
	git_annotated_commit *their_heads[SIMPLE_BRANCH_COUNT];
	git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	size_t i;

	static struct merge_index_entry merge_index_entries[] = {
		{ 0100644, "2c6e75856f24a210d21973441874bb1512fc5edd", 0, "1.txt" },
		{ 0100644, "374cd34a4752890de19d6c8a39510dbbe69406a5", 0, "added-in-branch1.txt" },
		{ 0100644, "527a828f16ab91bc9004269d2e3e3f30cbc8a854", 0, "added-in-branch2.txt" },
		{ 0100644, "dc61ca12000e7be2c4ea40398e783086428d053e", 0, "added-in-branch3.txt" },
		{ 0100644, "1f7391f92b6a3792204e07e99f71f643cc35e7e1", 0, "master.txt" },
		{ 0100644, "4b4d41231929d23b3f1de89c00b339831ebaa2b4", 0, "t.txt" },
		{ 0100644, "f15c1d17d1ebef8adbdb7379cc3dcf7f48fa9cb5", 0, "t1.txt" }
	};

	for (i = 0; i < SIMPLE_BRANCH_COUNT; ++i) {
		cl_git_pass(git_oid_from_string(&their_oids[i], oid_strings[i], GIT_OID_SHA1));
		cl_git_pass(git_annotated_commit_lookup(&their_heads[i], repo, &their_oids[i]));
	}

	checkout_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_pass(git_merge(repo,
		(const git_annotated_commit **)their_heads,
		SIMPLE_BRANCH_COUNT, &merge_opts, &checkout_opts));

	for (i = 0; i < SIMPLE_BRANCH_COUNT; ++i)
		git_annotated_commit_free(their_heads[i]);

	cl_assert(merge_test_index(repo_index, merge_index_entries, 7));
}

#define COMPLEX_BRANCHES { \
	"5136b71930b78146dfbe5f4c080c54e05b1f884a", /* 1 */  \
	"e2161c7b0ef124afe04c553fbd6f8e8156b947f5", /* 1a */ \
	"6d7a0948633012aa9038274538f76c968497b2ea", /* 1b */ \
	"1b7ad0f7343ff9ce03703cc40cf775b0e9cc57fe", /* 2 */  \
	"a49a6a4c527b223a1bdc1ae45e1260e1d041bf12", /* 2a */ \
	"75f1c450c1196e953e1dbfab827765a19623c856", /* 3 */  \
	"026a849d3c17944b00bd2de4840591df30852769", /* t1 */ \
	"96e602252e180110495f303b6164a7d1158de595"  /* t2 */ \
}
#define COMPLEX_BRANCH_COUNT 8

void test_merge_octopus__complex(void)
{
	char *oid_strings[COMPLEX_BRANCH_COUNT] = COMPLEX_BRANCHES;
	git_oid their_oids[COMPLEX_BRANCH_COUNT];
	git_annotated_commit *their_heads[COMPLEX_BRANCH_COUNT];
	git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	size_t i;

	static struct merge_index_entry merge_index_entries[] = {
	        { 0100644, "e7c1228a15149b7459531590842ff5e610e1a5c5", 0, "1.txt" },
		{ 0100644, "5ba15720d00755ff42ae0b7a3628c08326958ca7", 0, "1a.txt" },
		{ 0100644, "2481a2cc662ce05a7f0e52bd283403654a24d61c", 0, "1b.txt" },
		{ 0100644, "b2b120f3b488e6f80674f6b5c89aaec152485c66", 0, "2.txt" },
		{ 0100644, "09f4002ed8b3d379ac0f9322f9679c1006172bc3", 0, "2a.txt" },
		{ 0100644, "88a56d9ad6353e551de6d5025348e413b1c5d13f", 0, "3.txt" },
		{ 0100644, "1f7391f92b6a3792204e07e99f71f643cc35e7e1", 0, "master.txt" },
		{ 0100644, "4b4d41231929d23b3f1de89c00b339831ebaa2b4", 0, "t.txt" },
		{ 0100644, "ccfe29d825cb6476a3b1bf27d68a3edd4fd86c0b", 0, "t1.txt" },
		{ 0100644, "5fe609a987f5e38b5145ca136d5a0768629da47c", 0, "t2.txt" }
	};

	for (i = 0; i < COMPLEX_BRANCH_COUNT; ++i) {
		cl_git_pass(git_oid_from_string(&their_oids[i], oid_strings[i], GIT_OID_SHA1));
		cl_git_pass(git_annotated_commit_lookup(&their_heads[i], repo, &their_oids[i]));
	}

	checkout_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_pass(git_merge(repo,
		(const git_annotated_commit **)their_heads,
		COMPLEX_BRANCH_COUNT, &merge_opts, &checkout_opts));

	for (i = 0; i < COMPLEX_BRANCH_COUNT; ++i)
		git_annotated_commit_free(their_heads[i]);

	cl_assert(merge_test_index(repo_index, merge_index_entries, 10));
}

#define FFSKIP_BRANCHES { \
	"6c9e78d45edf0797f6b5af9a3e3241230c8ce65a", /* ff */        \
	"3e25ef4341e1ba0013f2fa65a9bf7298923180c5"  /* skippable */ \
}
#define FFSKIP_BRANCH_COUNT 2

void test_merge_octopus__fastforward_and_skip(void)
{
	char *oid_strings[FFSKIP_BRANCH_COUNT] = FFSKIP_BRANCHES;
	git_oid their_oids[FFSKIP_BRANCH_COUNT];
	git_annotated_commit *their_heads[FFSKIP_BRANCH_COUNT];
	git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	size_t i;

	static struct merge_index_entry merge_index_entries[] = {
	        { 0100644, "2c6e75856f24a210d21973441874bb1512fc5edd", 0, "1.txt" },
	        { 0100644, "3852122c84437abfb91b03df90677d3f1e3dbcd6", 0, "ff.txt" },
	        { 0100644, "1f7391f92b6a3792204e07e99f71f643cc35e7e1", 0, "master.txt" },
	        { 0100644, "4b4d41231929d23b3f1de89c00b339831ebaa2b4", 0, "t.txt" },
	        { 0100644, "f15c1d17d1ebef8adbdb7379cc3dcf7f48fa9cb5", 0, "t1.txt" }
	};

	for (i = 0; i < FFSKIP_BRANCH_COUNT; ++i) {
		cl_git_pass(git_oid_from_string(&their_oids[i], oid_strings[i], GIT_OID_SHA1));
		cl_git_pass(git_annotated_commit_lookup(&their_heads[i], repo, &their_oids[i]));
	}

	checkout_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_pass(git_merge(repo,
		(const git_annotated_commit **)their_heads,
		FFSKIP_BRANCH_COUNT, &merge_opts, &checkout_opts));

	for (i = 0; i < FFSKIP_BRANCH_COUNT; ++i)
		git_annotated_commit_free(their_heads[i]);

	cl_assert(merge_test_index(repo_index, merge_index_entries, 5));
}

#define FFSKIPANDMERGE_BRANCHES { \
	"6c9e78d45edf0797f6b5af9a3e3241230c8ce65a", /* ff */        \
	"3e25ef4341e1ba0013f2fa65a9bf7298923180c5", /* skippable */ \
	"5136b71930b78146dfbe5f4c080c54e05b1f884a"  /* 1 */         \
}
#define FFSKIPANDMERGE_BRANCH_COUNT 3

void test_merge_octopus__fastforward_skip_and_merge(void)
{
	char *oid_strings[FFSKIPANDMERGE_BRANCH_COUNT] = FFSKIPANDMERGE_BRANCHES;
	git_oid their_oids[FFSKIPANDMERGE_BRANCH_COUNT];
	git_annotated_commit *their_heads[FFSKIPANDMERGE_BRANCH_COUNT];
	git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	size_t i;

	static struct merge_index_entry merge_index_entries[] = {
	        { 0100644, "e7c1228a15149b7459531590842ff5e610e1a5c5", 0, "1.txt" },
	        { 0100644, "67e43930989305efbe75ac598126259707078305", 0, "1b.txt" },
	        { 0100644, "3852122c84437abfb91b03df90677d3f1e3dbcd6", 0, "ff.txt" },
	        { 0100644, "1f7391f92b6a3792204e07e99f71f643cc35e7e1", 0, "master.txt" },
	        { 0100644, "4b4d41231929d23b3f1de89c00b339831ebaa2b4", 0, "t.txt" },
	        { 0100644, "f15c1d17d1ebef8adbdb7379cc3dcf7f48fa9cb5", 0, "t1.txt" }
	};

	for (i = 0; i < FFSKIPANDMERGE_BRANCH_COUNT; ++i) {
		cl_git_pass(git_oid_from_string(&their_oids[i], oid_strings[i], GIT_OID_SHA1));
		cl_git_pass(git_annotated_commit_lookup(&their_heads[i], repo, &their_oids[i]));
	}

	checkout_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_pass(git_merge(repo,
		(const git_annotated_commit **)their_heads,
		FFSKIPANDMERGE_BRANCH_COUNT, &merge_opts, &checkout_opts));

	for (i = 0; i < FFSKIPANDMERGE_BRANCH_COUNT; ++i)
		git_annotated_commit_free(their_heads[i]);

	cl_assert(merge_test_index(repo_index, merge_index_entries, 6));
}

#define CONFLICT_BRANCHES { \
	"39e46a1032fdba5ab3876942af0b1959029c6b68", /* f1 */ \
	"56c73e2c30b2a5580821456409b8624ae4442495", /* f2 */ \
	"d5340b76365ef4f593a09428fcd155299b5c4523"  /* f3 */ \
}
#define CONFLICT_BRANCH_COUNT 3

void test_merge_octopus__conflicts_fail_to_merge(void)
{
	char *oid_strings[CONFLICT_BRANCH_COUNT] = CONFLICT_BRANCHES;
	git_oid their_oids[CONFLICT_BRANCH_COUNT];
	git_annotated_commit *their_heads[CONFLICT_BRANCH_COUNT];
	git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	size_t i;

	for (i = 0; i < CONFLICT_BRANCH_COUNT; ++i) {
		cl_git_pass(git_oid_from_string(&their_oids[i], oid_strings[i], GIT_OID_SHA1));
		cl_git_pass(git_annotated_commit_lookup(&their_heads[i], repo, &their_oids[i]));
	}

	merge_opts.file_favor = 0;
	checkout_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_fail_with(GIT_EMERGECONFLICT, git_merge(repo,
		(const git_annotated_commit **)their_heads,
		CONFLICT_BRANCH_COUNT, &merge_opts, &checkout_opts));

	for (i = 0; i < CONFLICT_BRANCH_COUNT; ++i)
		git_annotated_commit_free(their_heads[i]);
}
