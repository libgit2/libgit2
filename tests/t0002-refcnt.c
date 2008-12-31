#include "test_lib.h"
#include "common.h"

BEGIN_TEST(init_inc2_dec2_free)
	git_refcnt p;

	gitrc_init(&p);
	gitrc_inc(&p);
	gitrc_inc(&p);
	must_be_true(!gitrc_dec(&p));
	must_be_true(gitrc_dec(&p));
	gitrc_free(&p);
END_TEST
