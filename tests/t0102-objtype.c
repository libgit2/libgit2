
#include "test_lib.h"
#include <git/odb.h>

BEGIN_TEST(type_to_string)
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ_BAD), ""));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ__EXT1), ""));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ_COMMIT), "commit"));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ_TREE), "tree"));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ_BLOB), "blob"));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ_TAG), "tag"));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ__EXT2), ""));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ_OFS_DELTA), "OFS_DELTA"));
	must_be_true(!strcmp(git_obj_type_to_string(GIT_OBJ_REF_DELTA), "REF_DELTA"));

	must_be_true(!strcmp(git_obj_type_to_string(-2), ""));
	must_be_true(!strcmp(git_obj_type_to_string(8), ""));
	must_be_true(!strcmp(git_obj_type_to_string(1234), ""));
END_TEST

BEGIN_TEST(string_to_type)
	must_be_true(git_obj_string_to_type(NULL) == GIT_OBJ_BAD);
	must_be_true(git_obj_string_to_type("") == GIT_OBJ_BAD);
	must_be_true(git_obj_string_to_type("commit") == GIT_OBJ_COMMIT);
	must_be_true(git_obj_string_to_type("tree") == GIT_OBJ_TREE);
	must_be_true(git_obj_string_to_type("blob") == GIT_OBJ_BLOB);
	must_be_true(git_obj_string_to_type("tag") == GIT_OBJ_TAG);
	must_be_true(git_obj_string_to_type("OFS_DELTA") == GIT_OBJ_OFS_DELTA);
	must_be_true(git_obj_string_to_type("REF_DELTA") == GIT_OBJ_REF_DELTA);

	must_be_true(git_obj_string_to_type("CoMmIt") == GIT_OBJ_BAD);
	must_be_true(git_obj_string_to_type("hohoho") == GIT_OBJ_BAD);
END_TEST

BEGIN_TEST(loose_object)
	must_be_true(git_obj__loose_object_type(GIT_OBJ_BAD) == 0);
	must_be_true(git_obj__loose_object_type(GIT_OBJ__EXT1) == 0);
	must_be_true(git_obj__loose_object_type(GIT_OBJ_COMMIT) == 1);
	must_be_true(git_obj__loose_object_type(GIT_OBJ_TREE) == 1);
	must_be_true(git_obj__loose_object_type(GIT_OBJ_BLOB) == 1);
	must_be_true(git_obj__loose_object_type(GIT_OBJ_TAG) == 1);
	must_be_true(git_obj__loose_object_type(GIT_OBJ__EXT2) == 0);
	must_be_true(git_obj__loose_object_type(GIT_OBJ_OFS_DELTA) == 0);
	must_be_true(git_obj__loose_object_type(GIT_OBJ_REF_DELTA) == 0);

	must_be_true(git_obj__loose_object_type(-2) == 0);
	must_be_true(git_obj__loose_object_type(8) == 0);
	must_be_true(git_obj__loose_object_type(1234) == 0);
END_TEST

