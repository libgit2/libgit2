
#include "test_lib.h"
#include <git/odb.h>

BEGIN_TEST(type_to_string)
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ_BAD), ""));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ__EXT1), ""));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ_COMMIT), "commit"));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ_TREE), "tree"));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ_BLOB), "blob"));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ_TAG), "tag"));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ__EXT2), ""));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ_OFS_DELTA), "OFS_DELTA"));
	must_be_true(!strcmp(git_otype_tostring(GIT_OBJ_REF_DELTA), "REF_DELTA"));

	must_be_true(!strcmp(git_otype_tostring(-2), ""));
	must_be_true(!strcmp(git_otype_tostring(8), ""));
	must_be_true(!strcmp(git_otype_tostring(1234), ""));
END_TEST

BEGIN_TEST(string_to_type)
	must_be_true(git_otype_fromstring(NULL) == GIT_OBJ_BAD);
	must_be_true(git_otype_fromstring("") == GIT_OBJ_BAD);
	must_be_true(git_otype_fromstring("commit") == GIT_OBJ_COMMIT);
	must_be_true(git_otype_fromstring("tree") == GIT_OBJ_TREE);
	must_be_true(git_otype_fromstring("blob") == GIT_OBJ_BLOB);
	must_be_true(git_otype_fromstring("tag") == GIT_OBJ_TAG);
	must_be_true(git_otype_fromstring("OFS_DELTA") == GIT_OBJ_OFS_DELTA);
	must_be_true(git_otype_fromstring("REF_DELTA") == GIT_OBJ_REF_DELTA);

	must_be_true(git_otype_fromstring("CoMmIt") == GIT_OBJ_BAD);
	must_be_true(git_otype_fromstring("hohoho") == GIT_OBJ_BAD);
END_TEST

BEGIN_TEST(loose_object)
	must_be_true(git_otype_is_loose(GIT_OBJ_BAD) == 0);
	must_be_true(git_otype_is_loose(GIT_OBJ__EXT1) == 0);
	must_be_true(git_otype_is_loose(GIT_OBJ_COMMIT) == 1);
	must_be_true(git_otype_is_loose(GIT_OBJ_TREE) == 1);
	must_be_true(git_otype_is_loose(GIT_OBJ_BLOB) == 1);
	must_be_true(git_otype_is_loose(GIT_OBJ_TAG) == 1);
	must_be_true(git_otype_is_loose(GIT_OBJ__EXT2) == 0);
	must_be_true(git_otype_is_loose(GIT_OBJ_OFS_DELTA) == 0);
	must_be_true(git_otype_is_loose(GIT_OBJ_REF_DELTA) == 0);

	must_be_true(git_otype_is_loose(-2) == 0);
	must_be_true(git_otype_is_loose(8) == 0);
	must_be_true(git_otype_is_loose(1234) == 0);
END_TEST

