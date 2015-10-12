#include "clar_libgit2.h"
#include "backends.h"

int payload_target;

int odb_ctor(void *payload)
{
	GIT_UNUSED(payload);

	return 0;
}

void test_odb_registration__register(void)
{
	git_odb_registration *reg;

	cl_git_pass(git_odb_backend_register("foo", odb_ctor, &payload_target));
	reg = git_odb_backend__find("foo");

	cl_assert(reg);
	cl_assert_equal_s("foo", reg->name);
	cl_assert_equal_p(odb_ctor, reg->ctor);
	cl_assert_equal_p(&payload_target, reg->payload);

	reg = git_odb_backend__find("bar");
	cl_assert_equal_p(NULL, reg);
}
