#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"

#include <git/odb.h>
#include <git/commit.h>
#include <git/revwalk.h>

static const char *odb_dir = "../t0501-objects";
/*
    $ git log --oneline --graph --decorate
    *   a4a7dce (HEAD, br2) Merge branch 'master' into br2
    |\
    | * 9fd738e (master) a fourth commit
    | * 4a202b3 a third commit
    * | c47800c branch commit one
    |/
    * 5b5b025 another commit
    * 8496071 testing
*/
static const char *commit_head = "a4a7dce85cf63874e984719f4fdd239f5145052f";

static const char *commit_ids[] = {
    "a4a7dce85cf63874e984719f4fdd239f5145052f", /* 0 */
    "9fd738e8f7967c078dceed8190330fc8648ee56a", /* 1 */
    "4a202b346bb0fb0db7eff3cffeb3c70babbd2045", /* 2 */
    "c47800c7266a2be04c571c04d5a6614691ea99bd", /* 3 */
    "8496071c1b46c854b31185ea97743be6a8774479", /* 4 */
    "5b5b025afb0b4c913b4c338a42934a3863bf3644", /* 5 */
};

static const int commit_sorting_topo[] = {0, 1, 2, 3, 5, 4};
static const int commit_sorting_time[] = {0, 3, 1, 2, 5, 4};
static const int commit_sorting_topo_reverse[] = {4, 5, 3, 2, 1, 0};
static const int commit_sorting_time_reverse[] = {4, 5, 2, 1, 3, 0};
static const int commit_sorting_topo_time[] = {0};

BEGIN_TEST(simple_walk_test)
    git_odb *db;
    git_oid id;
    git_revpool *pool;
    git_commit *head = NULL;

    must_pass(git_odb_open(&db, odb_dir));

    pool = gitrp_alloc(db);
    must_be_true(pool != NULL);

    git_oid_mkstr(&id, commit_head);

    head = git_commit_parse(pool, &id);
    must_be_true(head != NULL);

    gitrp_push(pool, head);

#define TEST_WALK(sort_flags, result_array) {\
    char oid[40]; int i = 0;\
    git_commit *commit = NULL;\
    gitrp_sorting(pool, sort_flags);\
    while ((commit = gitrp_next(pool)) != NULL) {\
        git_oid_fmt(oid, &commit->object.id);\
        must_be_true(memcmp(oid, commit_ids[result_array[i++]], 40) == 0);\
    }\
    must_be_true(i == sizeof(result_array)/sizeof(int));\
    gitrp_reset(pool);\
}

    TEST_WALK(GIT_RPSORT_TIME, commit_sorting_time);
    TEST_WALK(GIT_RPSORT_TOPOLOGICAL, commit_sorting_topo);
    TEST_WALK(GIT_RPSORT_TIME | GIT_RPSORT_REVERSE, commit_sorting_time_reverse);
    TEST_WALK(GIT_RPSORT_TOPOLOGICAL | GIT_RPSORT_REVERSE, commit_sorting_topo_reverse);

#undef TEST_WALK

    gitrp_free(pool);
    git_odb_close(db);
END_TEST
