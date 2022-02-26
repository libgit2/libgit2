#include "clar_libgit2.h"

#define VALID_COMMIT "tree bdd24e358576f1baa275df98cdcaf3ac9a3f4233\n" \
               "parent d6d956f1d66210bfcd0484166befab33b5987a39\n" \
               "author Edward Thomson <ethomson@edwardthomson.com> 1638286404 -0500\n" \
               "committer Edward Thomson <ethomson@edwardthomson.com> 1638324642 -0500\n" \
               "\n" \
               "commit go here.\n"
#define VALID_TREE "100644 HEADER\0\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42"

#define INVALID_COMMIT "tree bdd24e358576f1baa275df98cdcaf3ac9a3f4233\n" \
               "parent d6d956f1d66210bfcd0484166befab33b5987a39\n" \
               "committer Edward Thomson <ethomson@edwardthomson.com> 1638324642 -0500\n" \
               "\n" \
               "commit go here.\n"
#define INVALID_TREE "100644 HEADER \x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42"

void test_object_validate__valid(void)
{
	int valid;

	cl_git_pass(git_object_rawcontent_is_valid(&valid, "", 0, GIT_OBJECT_BLOB));
	cl_assert_equal_i(1, valid);

	cl_git_pass(git_object_rawcontent_is_valid(&valid, "foobar", 0, GIT_OBJECT_BLOB));
	cl_assert_equal_i(1, valid);

	cl_git_pass(git_object_rawcontent_is_valid(&valid, VALID_COMMIT, CONST_STRLEN(VALID_COMMIT), GIT_OBJECT_COMMIT));
	cl_assert_equal_i(1, valid);

	cl_git_pass(git_object_rawcontent_is_valid(&valid, VALID_TREE, CONST_STRLEN(VALID_TREE), GIT_OBJECT_TREE));
	cl_assert_equal_i(1, valid);
}

void test_object_validate__invalid(void)
{
	int valid;

	cl_git_pass(git_object_rawcontent_is_valid(&valid, "", 0, GIT_OBJECT_COMMIT));
	cl_assert_equal_i(0, valid);

	cl_git_pass(git_object_rawcontent_is_valid(&valid, "foobar", 0, GIT_OBJECT_COMMIT));
	cl_assert_equal_i(0, valid);

	cl_git_pass(git_object_rawcontent_is_valid(&valid, INVALID_COMMIT, CONST_STRLEN(INVALID_COMMIT), GIT_OBJECT_COMMIT));
	cl_assert_equal_i(0, valid);

	cl_git_pass(git_object_rawcontent_is_valid(&valid, INVALID_TREE, CONST_STRLEN(INVALID_TREE), GIT_OBJECT_TREE));
	cl_assert_equal_i(0, valid);
}
