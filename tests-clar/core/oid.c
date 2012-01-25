#include "clar_libgit2.h"

static git_oid id;
const char *str_oid = "ae90f12eea699729ed24555e40b9fd669da12a12";

void test_core_oid__initialize(void)
{
	cl_git_pass(git_oid_fromstr(&id, str_oid));
}

void test_core_oid__streq(void)
{
	cl_assert(git_oid_streq(&id, str_oid) == GIT_SUCCESS);
	cl_assert(git_oid_streq(&id, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef") == GIT_ERROR);

	cl_assert(git_oid_streq(&id, "deadbeef") == GIT_ENOTOID);
	cl_assert(git_oid_streq(&id, "I'm not an oid.... :)") == GIT_ENOTOID);
}
