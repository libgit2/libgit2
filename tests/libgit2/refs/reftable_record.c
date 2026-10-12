#include "clar_libgit2.h"

#include "record.h"
#include "reftable-constants.h"
#include "reftable-error.h"

void test_refs_reftable_record__obj_with_more_offsets_than_bytes(void)
{
	struct reftable_buf key = REFTABLE_BUF_INIT, scratch = REFTABLE_BUF_INIT;
	struct reftable_record rec;
	uint8_t data[32];
	struct string_view out = { data, sizeof(data) }, in;
	int n;

	/*
	 * A count of offsets that a 32-bit size_t truncates to one,
	 * followed by two offsets.
	 */
	cl_assert((n = put_var_int(&out, ((uint64_t)1 << 32) + 1)) > 0);
	string_view_consume(&out, n);
	cl_assert((n = put_var_int(&out, 1)) > 0);
	string_view_consume(&out, n);
	cl_assert((n = put_var_int(&out, 1)) > 0);
	string_view_consume(&out, n);

	in.buf = data;
	in.len = sizeof(data) - out.len;

	cl_assert_equal_i(0, reftable_buf_addstr(&key, "\x01\x02"));
	cl_assert_equal_i(0, reftable_record_init(&rec, REFTABLE_BLOCK_TYPE_OBJ));
	cl_assert_equal_i(REFTABLE_FORMAT_ERROR,
		reftable_record_decode(&rec, key, 0, in, GIT_OID_SHA1_SIZE, &scratch));

	reftable_record_release(&rec);
	reftable_buf_release(&key);
	reftable_buf_release(&scratch);
}
