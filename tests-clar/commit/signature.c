#include "clar_libgit2.h"

static int try_build_signature(const char *name, const char *email, git_time_t time, int offset)
{
	git_signature *sign;
	int error = 0;

	if ((error =  git_signature_new(&sign, name, email, time, offset)) < 0)
		return error;

	git_signature_free((git_signature *)sign);

	return error;
}


void test_commit_signature__create_trim(void)
{
   // creating a signature trims leading and trailing spaces
   git_signature *sign;
	cl_git_pass(git_signature_new(&sign, "  nulltoken ", "   emeric.fermas@gmail.com     ", 1234567890, 60));
	cl_assert(strcmp(sign->name, "nulltoken") == 0);
	cl_assert(strcmp(sign->email, "emeric.fermas@gmail.com") == 0);
	git_signature_free((git_signature *)sign);
}


void test_commit_signature__create_empties(void)
{
   // can not create a signature with empty name or email
	cl_git_pass(try_build_signature("nulltoken", "emeric.fermas@gmail.com", 1234567890, 60));

	cl_git_fail(try_build_signature("", "emeric.fermas@gmail.com", 1234567890, 60));
	cl_git_fail(try_build_signature("   ", "emeric.fermas@gmail.com", 1234567890, 60));
	cl_git_fail(try_build_signature("nulltoken", "", 1234567890, 60));
	cl_git_fail(try_build_signature("nulltoken", "  ", 1234567890, 60));
}

void test_commit_signature__create_one_char(void)
{
   // creating a one character signature
	git_signature *sign;
	cl_git_pass(git_signature_new(&sign, "x", "foo@bar.baz", 1234567890, 60));
	cl_assert(strcmp(sign->name, "x") == 0);
	cl_assert(strcmp(sign->email, "foo@bar.baz") == 0);
	git_signature_free((git_signature *)sign);
}

void test_commit_signature__create_two_char(void)
{
   // creating a two character signature
	git_signature *sign;
	cl_git_pass(git_signature_new(&sign, "xx", "x@y.z", 1234567890, 60));
	cl_assert(strcmp(sign->name, "xx") == 0);
	cl_assert(strcmp(sign->email, "x@y.z") == 0);
	git_signature_free((git_signature *)sign);
}

void test_commit_signature__create_zero_char(void)
{
   // creating a zero character signature
	git_signature *sign;
	cl_git_fail(git_signature_new(&sign, "", "x@y.z", 1234567890, 60));
	cl_assert(sign == NULL);
}
