#include "clar_libgit2.h"

void test_refs_tags_is_name_valid(void)
{
	cl_assert_equal_i(true, git_branch_is_name_valid("sometag"));
	cl_assert_equal_i(true, git_branch_is_name_valid("test/sometag"));

	cl_assert_equal_i(false, git_branch_is_name_valid(""));
	cl_assert_equal_i(false, git_branch_is_name_valid("-dash"));
}
