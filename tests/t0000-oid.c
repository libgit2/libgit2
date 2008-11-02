#include "test_lib.h"
#include <git/oid.h>

BEGIN_TEST(empty_string)
	git_oid out;
	must_fail(git_oid_mkstr(&out, ""));
END_TEST

BEGIN_TEST(invalid_string_moo)
	git_oid out;
	must_fail(git_oid_mkstr(&out, "moo"));
END_TEST

BEGIN_TEST(invalid_string_16a67770b7d8d72317c4b775213c23a8bd74f5ez)
	git_oid out;
	must_fail(git_oid_mkstr(&out, "16a67770b7d8d72317c4b775213c23a8bd74f5ez"));
END_TEST

BEGIN_TEST(valid_string_16a67770b7d8d72317c4b775213c23a8bd74f5e0)
	git_oid out;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	must_pass(git_oid_mkstr(&out, "16a67770b7d8d72317c4b775213c23a8bd74f5e0"));
	must_pass(memcmp(out.id, exp, sizeof(out.id)));

	must_pass(git_oid_mkstr(&out, "16A67770B7D8D72317C4b775213C23A8BD74F5E0"));
	must_pass(memcmp(out.id, exp, sizeof(out.id)));
END_TEST

BEGIN_TEST(valid_raw)
	git_oid out;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	git_oid_mkraw(&out, exp);
	must_pass(memcmp(out.id, exp, sizeof(out.id)));
END_TEST
