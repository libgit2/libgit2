#include "clar_libgit2.h"

static git_oid id;
const char *str_oid = "ae90f12eea699729ed24555e40b9fd669da12a12";

void test_core_oid__initialize(void)
{
	cl_git_pass(git_oid_fromstr(&id, str_oid));
}

void test_core_oid__streq(void)
{
	cl_assert(git_oid_streq(&id, str_oid) == 0);
	cl_assert(git_oid_streq(&id, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef") == -1);

	cl_assert(git_oid_streq(&id, "deadbeef") == -1);
	cl_assert(git_oid_streq(&id, "I'm not an oid.... :)") == -1);
}
