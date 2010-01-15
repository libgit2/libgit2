#include "test_lib.h"
#include "errors.h"

BEGIN_TEST(errno_zero_on_init)
	must_be_true(git_errno == 0);
END_TEST

BEGIN_TEST(set_ENOTOID)
	must_be_true(GIT_ENOTOID != 0);
	git_errno = GIT_ENOTOID;
	must_be_true(git_errno == GIT_ENOTOID);
	must_pass(strcmp(git_strerror(git_errno), "Not a git oid"));
END_TEST
