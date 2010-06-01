#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include <git/odb.h>
#include <git/commit.h>

BEGIN_TEST(list_timesort_test)

    git_commit_list list;
    git_commit_node *n;
    int i, t;
    time_t previous_time;

#define TEST_SORTED() \
    previous_time = INT_MAX;\
    for (n = list.head; n != NULL; n = n->next) {\
        must_be_true(n->commit->commit_time <= previous_time);\
        previous_time = n->commit->commit_time;\
    }

    memset(&list, 0x0, sizeof(git_commit_list));
    srand((unsigned int)time(NULL));

    for (t = 0; t < 20; ++t) {
        const int test_size = rand() % 500 + 500;

        /* Purely random sorting test */
        for (i = 0; i < test_size; ++i) {
            git_commit *c = git__malloc(sizeof(git_commit));
            c->commit_time = (time_t)rand();

            git_commit_list_push_back(&list, c);
        }

        git_commit_list_timesort(&list);
        TEST_SORTED();
        git_commit_list_clear(&list, 1);
    }

    /* Try to sort list with all dates equal. */
    for (i = 0; i < 200; ++i) {
        git_commit *c = git__malloc(sizeof(git_commit));
        c->commit_time = 0;

        git_commit_list_push_back(&list, c);
    }

    git_commit_list_timesort(&list);
    TEST_SORTED();
    git_commit_list_clear(&list, 1);

    /* Try to sort empty list */
    git_commit_list_timesort(&list);
    TEST_SORTED();

END_TEST
