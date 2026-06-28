#include "clar_libgit2.h"
#include "odb.h"
#include "zstream.h"

#define DELTA_2 "8dfd652805e877abaca7383ad28d8eaa5b9a7e04"
#define DELTA_1 "faf2dae9a5d206471233bfa8698ecbdfb24785d1"
#define DELTA_BASE "4d25aed8f9ae7653206031efdb0b682d62ece767"

static const unsigned char delta_2_expected[] = {
	0xed, 0x03, 0xed, 0x03, 0x90, 0x2b, 0x07, 0x69,
	0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0xb1, 0x32,
	0xbb, 0x01,
};

static const unsigned char delta_1_expected[] = {
	0x87, 0x04, 0xed, 0x03, 0xb0, 0xa9, 0x01, 0x93,
	0xc3, 0x01, 0x44,
};

static git_odb *odb;

void test_odb_delta__initialize(void)
{
	cl_git_pass(git_odb_open_ext(&odb, cl_fixture("testrepo.git/objects"), NULL));
}

void test_odb_delta__cleanup(void)
{
	git_odb_free(odb);
	odb = NULL;
}

static void check_delta(
        const char *oid,
        const char *expected_base,
        const unsigned char expected_contents[],
        size_t expected_size)
{
	git_oid id, base_id;
	void *z_delta;
	size_t size;
	size_t z_size;
	git_str inflated = GIT_STR_INIT;

	cl_git_pass(git_oid_from_string(&id, oid, GIT_OID_SHA1));

	cl_git_pass(git_odb__get_delta(&base_id, &z_delta, &size, &z_size, odb, &id));

	cl_assert_equal_s(git_oid_tostr_s(&base_id), expected_base);

	cl_assert_equal_i(size, expected_size);
	cl_assert_equal_i(z_size, expected_size + 8);

	cl_git_pass(git_zstream_inflatebuf(&inflated, z_delta, z_size));
	cl_assert_equal_i(inflated.size, expected_size);
	cl_assert(memcmp(inflated.ptr, expected_contents, expected_size) == 0);

	git_str_dispose(&inflated);
	git__free(z_delta);
}

void test_odb_delta__get_delta_against_delta(void)
{
	/*
	 * DELTA_2 is stored as a delta against DELTA_1
	 */
	check_delta(DELTA_2, DELTA_1, delta_2_expected, sizeof(delta_2_expected));
}

void test_odb_delta__get_delta_against_base(void)
{
	/*
	 * DELTA_1 is stored as a delta against DELTA_BASE
	 */
	check_delta(DELTA_1, DELTA_BASE, delta_1_expected, sizeof(delta_1_expected));
}

void test_odb_delta__get_delta_for_non_delta(void)
{
	git_oid id, base_id;
	void *z_delta;
	size_t size;
	size_t z_size;

	/*
	 * DELTA_BASE is not stored as a delta
	 */
	cl_git_pass(git_oid_from_string(&id, DELTA_BASE, GIT_OID_SHA1));
	cl_git_fail(git_odb__get_delta(&base_id, &z_delta, &size, &z_size, odb, &id));
}
