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

#define THEIRS_SIMPLE_BRANCHES		{"f1", "f2", "f3"}
#define THEIRS_SIMPLE_OIDS			{"39e46a1032fdba5ab3876942af0b1959029c6b68", \
                                     "56c73e2c30b2a5580821456409b8624ae4442495", \
                                     "d5340b76365ef4f593a09428fcd155299b5c4523"}

#define OUR_TARGET_BRANCH "t"
#define OUR_TARGET_OID "ec7080d7b13802e78dc64ef04b4ff218f7f3a06b"
#define NUM_COMMITS 3


/* Fixture setup and teardown */
void test_merge_octopus_conflict__initialize(void)
{
	git_config *cfg;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);

	/* Ensure that the user's merge.conflictstyle doesn't interfere */
	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_string(cfg, "merge.conflictstyle", "merge"));
	git_config_free(cfg);
}

void test_merge_octopus_conflicconflicteanup(void)
{
	git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

void test_merge_octopus_conflict__fail_to_merge(void)
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

	merge_opts.file_favor = 0;
	checkout_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_fail_with(GIT_EMERGECONFLICT, git_merge(repo, (const git_annotated_commit **)their_heads, NUM_COMMITS, &merge_opts, &checkout_opts));

    for(i = 0; i < NUM_COMMITS; ++i) {
        git_annotated_commit_free(their_heads[i]);
    }
}

