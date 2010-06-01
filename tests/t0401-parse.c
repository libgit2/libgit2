#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include <git/odb.h>
#include <git/commit.h>
#include <git/revwalk.h>

static char *test_commits_broken[] = {

    /* empty commit */
"",

    /* random garbage */
"asd97sa9du902e9a0jdsuusad09as9du098709aweu8987sd\n",

    /* broken endlines 1 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\r\n\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\r\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\r\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\r\n\
\r\n\
a test commit with broken endlines\r\n",

    /* broken endlines 2 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\
\
another test commit with broken endlines",

    /* starting endlines */
"\ntree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
a test commit with a starting endline\n",

    /* corrupted commit 1 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent 05452d6349abcd67aa396df",

    /* corrupted commit 2 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent ",

    /* corrupted commit 3 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent ",

    /* corrupted commit 4 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
par",

    /*
     * FIXME: duplicated parents?
     * It this supposed to pass?
     */
/*
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\n\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
duplicated parent",
*/
};


static char *test_commits_working[] = {
    /* simple commit with no message */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n",

    /* simple commit, no parent */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
a simple commit which works\n",

    /* simple commit, 1 parents */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
a simple commit which works\n",
};

BEGIN_TEST(parse_oid_test)

    git_oid oid;

#define TEST_OID_PASS(string, header) { \
    char *ptr = string;\
    char *ptr_original = ptr;\
    size_t len = strlen(ptr);\
    must_pass(git_commit__parse_oid(&oid, &ptr, ptr + len, header));\
    must_be_true(ptr == ptr_original + len);\
}

#define TEST_OID_FAIL(string, header) { \
    char *ptr = string;\
    size_t len = strlen(ptr);\
    must_fail(git_commit__parse_oid(&oid, &ptr, ptr + len, header));\
}

    TEST_OID_PASS("parent 05452d6349abcd67aa396dfb28660d765d8b2a36\n", "parent ");
    TEST_OID_PASS("tree 05452d6349abcd67aa396dfb28660d765d8b2a36\n", "tree ");
    TEST_OID_PASS("random_heading 05452d6349abcd67aa396dfb28660d765d8b2a36\n", "random_heading ");
    TEST_OID_PASS("stuck_heading05452d6349abcd67aa396dfb28660d765d8b2a36\n", "stuck_heading");
    TEST_OID_PASS("tree 5F4BEFFC0759261D015AA63A3A85613FF2F235DE\n", "tree ");
    TEST_OID_PASS("tree 1A669B8AB81B5EB7D9DB69562D34952A38A9B504\n", "tree ");
    TEST_OID_PASS("tree 5B20DCC6110FCC75D31C6CEDEBD7F43ECA65B503\n", "tree ");
    TEST_OID_PASS("tree 173E7BF00EA5C33447E99E6C1255954A13026BE4\n", "tree ");

    TEST_OID_FAIL("parent 05452d6349abcd67aa396dfb28660d765d8b2a36", "parent ");
    TEST_OID_FAIL("05452d6349abcd67aa396dfb28660d765d8b2a36\n", "tree ");
    TEST_OID_FAIL("parent05452d6349abcd67aa396dfb28660d765d8b2a6a\n", "parent ");
    TEST_OID_FAIL("parent 05452d6349abcd67aa396dfb280d765d8b2a6\n", "parent ");
    TEST_OID_FAIL("tree  05452d6349abcd67aa396dfb28660d765d8b2a36\n", "tree ");
    TEST_OID_FAIL("parent 0545xd6349abcd67aa396dfb28660d765d8b2a36\n", "parent ");
    TEST_OID_FAIL("parent 0545xd6349abcd67aa396dfb28660d765d8b2a36FF\n", "parent ");
    TEST_OID_FAIL("", "tree ");
    TEST_OID_FAIL("", "");

#undef TEST_OID_PASS
#undef TEST_OID_FAIL

END_TEST

BEGIN_TEST(parse_buffer_test)
    const int broken_commit_count = sizeof(test_commits_broken) / sizeof(*test_commits_broken);
    const int working_commit_count = sizeof(test_commits_working) / sizeof(*test_commits_working);
    int i;

    git_revpool *pool = gitrp_alloc(NULL);

    for (i = 0; i < broken_commit_count; ++i) {
        git_commit commit;
        memset(&commit, 0x0, sizeof(git_commit));
        commit.object.pool = pool;

        must_fail(git_commit__parse_buffer(
                    &commit,
                    test_commits_broken[i],
                    strlen(test_commits_broken[i]))
                );
    }

    for (i = 0; i < working_commit_count; ++i) {
        git_commit commit;
        memset(&commit, 0x0, sizeof(git_commit));
        commit.object.pool = pool;

        must_pass(git_commit__parse_buffer(
                    &commit,
                    test_commits_working[i],
                    strlen(test_commits_working[i]))
                );
    }

    gitrp_free(pool);

END_TEST
