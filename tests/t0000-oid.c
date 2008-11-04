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

static int from_hex(unsigned char i)
{
	if (i >= '0' && i <= '9')
		return i - '0';
	if (i >= 'a' && i <= 'f')
		return 10 + (i - 'a');
	if (i >= 'A' && i <= 'F')
		return 10 + (i - 'A');
	return -1;
}

BEGIN_TEST(invalid_string_all_chars)
	git_oid out;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};
	char in[41] = "16a67770b7d8d72317c4b775213c23a8bd74f5e0\0";
	unsigned int i;

	for (i = 0; i < 256; i++) {
		in[38] = (char)i;

		if (from_hex(i) >= 0) {
			exp[19] = (from_hex(i) << 4);
			if (git_oid_mkstr(&out, in))
				test_die("line %d: must accept '%s'", __LINE__, in);
			if (memcmp(out.id, exp, sizeof(out.id)))
				test_die("line %d: bad parse of '%s', %x != %x",
				         __LINE__, in, exp[19], out.id[19]);
		} else if (!git_oid_mkstr(&out, in))
			test_die("line %d: must not accept '%s'", __LINE__, in);
	}
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

BEGIN_TEST(copy_oid)
	git_oid a, b;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	memset(&b, 0, sizeof(b));
	git_oid_mkraw(&a, exp);
	git_oid_cpy(&b, &a);
	must_pass(memcmp(a.id, exp, sizeof(a.id)));
END_TEST

BEGIN_TEST(cmp_oid_lt)
	git_oid a, b;
	unsigned char a_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};
	unsigned char b_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xf0,
	};

	git_oid_mkraw(&a, a_in);
	git_oid_mkraw(&b, b_in);
	must_be_true(git_oid_cmp(&a, &b) < 0);
END_TEST

BEGIN_TEST(cmp_oid_eq)
	git_oid a, b;
	unsigned char a_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	git_oid_mkraw(&a, a_in);
	git_oid_mkraw(&b, a_in);
	must_be_true(git_oid_cmp(&a, &b) == 0);
END_TEST

BEGIN_TEST(cmp_oid_gt)
	git_oid a, b;
	unsigned char a_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};
	unsigned char b_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xd0,
	};

	git_oid_mkraw(&a, a_in);
	git_oid_mkraw(&b, b_in);
	must_be_true(git_oid_cmp(&a, &b) > 0);
END_TEST
