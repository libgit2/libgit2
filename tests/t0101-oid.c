#include "test_lib.h"
#include <git/oid.h>

BEGIN_TEST(oid_szs)
	git_oid out;
	must_be_true(20 == GIT_OID_RAWSZ);
	must_be_true(40 == GIT_OID_HEXSZ);
	must_be_true(sizeof(out) == GIT_OID_RAWSZ);
	must_be_true(sizeof(out.id) == GIT_OID_RAWSZ);
END_TEST

BEGIN_TEST(empty_string)
	git_oid out;
	must_fail(git_oid_mkstr(&out, ""));
END_TEST

BEGIN_TEST(invalid_string_moo)
	git_oid out;
	must_fail(git_oid_mkstr(&out, "moo"));
END_TEST

static int from_hex(unsigned int i)
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
	char in[41] = "16a67770b7d8d72317c4b775213c23a8bd74f5e0";
	unsigned int i;

	for (i = 0; i < 256; i++) {
		in[38] = (char)i;

		if (from_hex(i) >= 0) {
			exp[19] = (unsigned char)(from_hex(i) << 4);
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

BEGIN_TEST(cmp_oid_fmt)
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char out[GIT_OID_HEXSZ + 1];

	must_pass(git_oid_mkstr(&in, exp));

	/* Format doesn't touch the last byte */
	out[GIT_OID_HEXSZ] = 'Z';
	git_oid_fmt(out, &in);
	must_be_true(out[GIT_OID_HEXSZ] == 'Z');

	/* Format produced the right result */
	out[GIT_OID_HEXSZ] = '\0';
	must_pass(strcmp(exp, out));
END_TEST

BEGIN_TEST(cmp_oid_allocfmt)
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char *out;

	must_pass(git_oid_mkstr(&in, exp));

	out = git_oid_allocfmt(&in);
	must_be_true(out);
	must_pass(strcmp(exp, out));
	free(out);
END_TEST

BEGIN_TEST(cmp_oid_pathfmt)
	const char *exp1 = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	const char *exp2 = "16/a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char out[GIT_OID_HEXSZ + 2];

	must_pass(git_oid_mkstr(&in, exp1));

	/* Format doesn't touch the last byte */
	out[GIT_OID_HEXSZ + 1] = 'Z';
	git_oid_pathfmt(out, &in);
	must_be_true(out[GIT_OID_HEXSZ + 1] == 'Z');

	/* Format produced the right result */
	out[GIT_OID_HEXSZ + 1] = '\0';
	must_pass(strcmp(exp2, out));
END_TEST

BEGIN_TEST(oid_to_string)
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char out[GIT_OID_HEXSZ + 1];
	char *str;
	int i;

	must_pass(git_oid_mkstr(&in, exp));

	/* NULL buffer pointer, returns static empty string */
	str = git_oid_to_string(NULL, sizeof(out), &in);
	must_be_true(str && *str == '\0' && str != out);

	/* zero buffer size, returns static empty string */
	str = git_oid_to_string(out, 0, &in);
	must_be_true(str && *str == '\0' && str != out);

	/* NULL oid pointer, returns static empty string */
	str = git_oid_to_string(out, sizeof(out), NULL);
	must_be_true(str && *str == '\0' && str != out);

	/* n == 1, returns out as an empty string */
	str = git_oid_to_string(out, 1, &in);
	must_be_true(str && *str == '\0' && str == out);

	for (i = 1; i < GIT_OID_HEXSZ; i++) {
		out[i+1] = 'Z';
		str = git_oid_to_string(out, i+1, &in);
		/* returns out containing c-string */
		must_be_true(str && str == out);
		/* must be '\0' terminated */
		must_be_true(*(str+i) == '\0');
		/* must not touch bytes past end of string */
		must_be_true(*(str+(i+1)) == 'Z');
		/* i == n-1 charaters of string */
		must_pass(strncmp(exp, out, i));
	}

	/* returns out as hex formatted c-string */
	str = git_oid_to_string(out, sizeof(out), &in);
	must_be_true(str && str == out && *(str+GIT_OID_HEXSZ) == '\0');
	must_pass(strcmp(exp, out));
END_TEST

