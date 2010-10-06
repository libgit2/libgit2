#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"

#include <git/odb.h>
#include <git/tag.h>
#include <git/revwalk.h>

static const char *odb_dir = "../resources/pack-odb";
static const char *tag_id = "b25fa35b38051e4ae45d4222e795f9df2e43f1d1";

BEGIN_TEST(tag_writeback_test)
	git_odb *db;
	git_oid id;
	git_repository *repo;
	git_tag *tag;
	/* char hex_oid[41]; */

	must_pass(git_odb_open(&db, odb_dir));

	repo = git_repository_alloc(db);
	must_be_true(repo != NULL);

	git_oid_mkstr(&id, tag_id);

	tag = git_tag_lookup(repo, &id);
	must_be_true(tag != NULL);

	git_tag_set_name(tag, "This is a different tag LOL");

	must_pass(git_object_write((git_object *)tag));

/*
	git_oid_fmt(hex_oid, git_tag_id(tag));
	hex_oid[40] = 0;
	printf("TAG New SHA1: %s\n", hex_oid);
*/

	must_pass(remove_loose_object(odb_dir, (git_object *)tag));

	git_repository_free(repo);
	git_odb_close(db);
END_TEST
