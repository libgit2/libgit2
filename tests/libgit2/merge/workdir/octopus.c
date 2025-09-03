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
#define THEIRS_SIMPLE_OIDS			{"a4f76792d9bbf5a939cfc43a7a0df48bd8f0efb2", \
                                     "d5b8b3e29080838047c80e139d46d381c8200253", \
                                     "b904bbfb2674a1df716feaf9cc30c51ca4e2f351", \
                                     "07f14b3a5eeb7d6db41f03bf7f29d5a404d9de16"}

#define OUR_TARGET_BRANCH "target"
#define NUM_COMMITS 4

#define THEIRS_UNRELATED_BRANCH		"unrelated"
#define THEIRS_UNRELATED_OID		"55b4e4687e7a0d9ca367016ed930f385d4022e6f"
#define THEIRS_UNRELATED_PARENT		"d6cf6c7741b3316826af1314042550c97ded1d50"

#define OURS_DIRECTORY_FILE			"df_side1"
#define THEIRS_DIRECTORY_FILE		"fc90237dc4891fa6c69827fc465632225e391618"



/* Fixture setup and teardown */
void test_merge_workdir_octopus__initialize(void)
{
	git_config *cfg;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);

	/* Ensure that the user's merge.conflictstyle doesn't interfere */
	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_string(cfg, "merge.conflictstyle", "merge"));
	git_config_free(cfg);
}

void test_merge_workdir_octopus__cleanup(void)
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
        git_annotated_commit_free(their_heads[0]);
    }
}

void test_merge_workdir_octopus__merge_multiple_commits(void)
{
	octopus_merge(0, 0);
}

