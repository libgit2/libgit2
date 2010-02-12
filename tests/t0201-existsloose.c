#include "test_lib.h"
#include "test_helpers.h"
#include <git/odb.h>

static char *odb_dir = "test-objects";

/* one == 8b137891791fe96927ad78e64b0aad7bded08bdc */
static unsigned char one_bytes[] = {
    0x31, 0x78, 0x9c, 0xe3, 0x02, 0x00, 0x00, 0x0b,
    0x00, 0x0b,
};

static unsigned char one_data[] = {
    0x0a,
};

static object_data one = {
    one_bytes,
    sizeof(one_bytes),
    "8b137891791fe96927ad78e64b0aad7bded08bdc",
    "blob",
    "test-objects/8b",
    "test-objects/8b/137891791fe96927ad78e64b0aad7bded08bdc",
    one_data,
    sizeof(one_data),
};


BEGIN_TEST(exists_loose_one)
    git_odb *db;
    git_oid id, id2;

    must_pass(write_object_files(odb_dir, &one));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, one.id));

    must_be_true(git_odb_exists(db, &id));

	/* Test for a non-existant object */
    must_pass(git_oid_mkstr(&id2, "8b137891791fe96927ad78e64b0aad7bded08baa"));
    must_be_true(0 == git_odb_exists(db, &id2));

    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &one));
END_TEST
