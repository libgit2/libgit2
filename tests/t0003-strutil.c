#include "test_lib.h"
#include "common.h"

BEGIN_TEST(prefixcmp_empty_empty)
	must_be_true(git__prefixcmp("", "") == 0);
END_TEST

BEGIN_TEST(prefixcmp_a_empty)
	must_be_true(git__prefixcmp("a", "") == 0);
END_TEST

BEGIN_TEST(prefixcmp_empty_a)
	must_be_true(git__prefixcmp("", "a") < 0);
END_TEST

BEGIN_TEST(prefixcmp_a_b)
	must_be_true(git__prefixcmp("a", "b") < 0);
END_TEST

BEGIN_TEST(prefixcmp_b_a)
	must_be_true(git__prefixcmp("b", "a") > 0);
END_TEST

BEGIN_TEST(prefixcmp_ab_a)
	must_be_true(git__prefixcmp("ab", "a") == 0);
END_TEST

BEGIN_TEST(prefixcmp_ab_ac)
	must_be_true(git__prefixcmp("ab", "ac") < 0);
END_TEST

BEGIN_TEST(prefixcmp_ab_aa)
	must_be_true(git__prefixcmp("ab", "aa") > 0);
END_TEST


BEGIN_TEST(suffixcmp_empty_empty)
	must_be_true(git__suffixcmp("", "") == 0);
END_TEST

BEGIN_TEST(suffixcmp_a_empty)
	must_be_true(git__suffixcmp("a", "") == 0);
END_TEST

BEGIN_TEST(suffixcmp_empty_a)
	must_be_true(git__suffixcmp("", "a") < 0);
END_TEST

BEGIN_TEST(suffixcmp_a_b)
	must_be_true(git__suffixcmp("a", "b") < 0);
END_TEST

BEGIN_TEST(suffixcmp_b_a)
	must_be_true(git__suffixcmp("b", "a") > 0);
END_TEST

BEGIN_TEST(suffixcmp_ba_a)
	must_be_true(git__suffixcmp("ba", "a") == 0);
END_TEST

BEGIN_TEST(suffixcmp_zaa_ac)
	must_be_true(git__suffixcmp("zaa", "ac") < 0);
END_TEST

BEGIN_TEST(suffixcmp_zaz_ac)
	must_be_true(git__suffixcmp("zaz", "ac") > 0);
END_TEST

BEGIN_TEST(dirname)
	char dir[64];

	must_be_true(!(git__dirname(dir, sizeof(dir), NULL) < 0));
	must_be_true(!strcmp(dir, "."));

	must_be_true(!(git__dirname(dir, sizeof(dir), "") < 0));
	must_be_true(!strcmp(dir, "."));

	must_be_true(!(git__dirname(dir, sizeof(dir), "a") < 0));
	must_be_true(!strcmp(dir, "."));

	must_be_true(!(git__dirname(dir, sizeof(dir), "/") < 0));
	must_be_true(!strcmp(dir, "/"));

	must_be_true(!(git__dirname(dir, sizeof(dir), "/usr") < 0));
	must_be_true(!strcmp(dir, "/"));

	/* TODO: should this be "/" instead (ie strip trailing / first) */
	must_be_true(!(git__dirname(dir, sizeof(dir), "/usr/") < 0));
	must_be_true(!strcmp(dir, "/usr"));

	must_be_true(!(git__dirname(dir, sizeof(dir), "/usr/lib") < 0));
	must_be_true(!strcmp(dir, "/usr"));

	must_be_true(!(git__dirname(dir, sizeof(dir), "usr/lib") < 0));
	must_be_true(!strcmp(dir, "usr"));
END_TEST

BEGIN_TEST(basename)
	char base[64];

	must_be_true(!(git__basename(base, sizeof(base), NULL) < 0));
	must_be_true(!strcmp(base, "."));

	must_be_true(!(git__basename(base, sizeof(base), "") < 0));
	must_be_true(!strcmp(base, "."));

	must_be_true(!(git__basename(base, sizeof(base), "a") < 0));
	must_be_true(!strcmp(base, "a"));

	must_be_true(!(git__basename(base, sizeof(base), "/") < 0));
	must_be_true(!strcmp(base, "/"));

	must_be_true(!(git__basename(base, sizeof(base), "/usr") < 0));
	must_be_true(!strcmp(base, "usr"));

	/* TODO: should this be "usr" instead (ie strip trailing / first) */
	must_be_true(!(git__basename(base, sizeof(base), "/usr/") < 0));
	must_be_true(!strcmp(base, ""));

	must_be_true(!(git__basename(base, sizeof(base), "/usr/lib") < 0));
	must_be_true(!strcmp(base, "lib"));

	must_be_true(!(git__basename(base, sizeof(base), "usr/lib") < 0));
	must_be_true(!strcmp(base, "lib"));
END_TEST
