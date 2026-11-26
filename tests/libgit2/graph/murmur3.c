#include "clar_libgit2.h"

#include "util.h"

/*
 * Vectors below tests both v1 and v2 used in git. v1 and v2 agree
 * on pure-ASCII input; they diverge once a byte >= 0x80 is involved, which is
 * the sign-extension bug bloom_filter_hash_version == 1 needs for git
 * compatibility.
 */

#define SEED1 0x293ae76fu
#define SEED2 0x7e646e2cu

static void assert_v1(const char *data, int len, uint32_t expected_h1, uint32_t expected_h2)
{
	cl_assert_equal_i(git__hash(data, len, SEED1), expected_h1);
	cl_assert_equal_i(git__hash(data, len, SEED2), expected_h2);
}

static void assert_v2(const char *data, int len, uint32_t expected_h1, uint32_t expected_h2)
{
	cl_assert_equal_i(git__hash_v2(data, len, SEED1), expected_h1);
	cl_assert_equal_i(git__hash_v2(data, len, SEED2), expected_h2);
}

void test_graph_murmur3__v1_empty(void)
{
	assert_v1("", 0, 0x5615800cu, 0x0580e554u);
}

void test_graph_murmur3__v2_empty(void)
{
	assert_v2("", 0, 0x5615800cu, 0x0580e554u);
}

void test_graph_murmur3__v1_ascii(void)
{
	assert_v1("a", 1, 0x8fc5291au, 0xf720b3beu);
	assert_v1("README", 6, 0x8003a218u, 0x02765271u);
	assert_v1("src/main.c", 10, 0xa27007c1u, 0xdb9c22cau);
}

void test_graph_murmur3__v2_ascii(void)
{
	assert_v2("a", 1, 0x8fc5291au, 0xf720b3beu);
	assert_v2("README", 6, 0x8003a218u, 0x02765271u);
	assert_v2("src/main.c", 10, 0xa27007c1u, 0xdb9c22cau);
}

/*
 * "docs/caf\xc3\xa9.txt" contains the UTF-8 encoding of "café.txt",
 * whose continuation bytes are >= 0x80.
 */
void test_graph_murmur3__v1_high_bit_tail(void)
{
	static const char data[] = "docs/caf\xc3\xa9.txt";
	assert_v1(data, sizeof(data) - 1, 0x669bbcf2u, 0x7c7e0706u);
}

void test_graph_murmur3__v2_high_bit_tail(void)
{
	static const char data[] = "docs/caf\xc3\xa9.txt";
	assert_v2(data, sizeof(data) - 1, 0x43eb492bu, 0xf0def225u);
}

void test_graph_murmur3__v1_high_bit_full_block(void)
{
	static const char data[] = "\xff\xfe\xfd\xfc";
	assert_v1(data, sizeof(data) - 1, 0xffef0993u, 0x1b924c27u);
}

void test_graph_murmur3__v2_high_bit_full_block(void)
{
	static const char data[] = "\xff\xfe\xfd\xfc";
	assert_v2(data, sizeof(data) - 1, 0xff1db74cu, 0xa592c26du);
}

void test_graph_murmur3__v1_high_bit_short_tail(void)
{
	static const char data[] = "\x80";
	assert_v1(data, sizeof(data) - 1, 0xa0eaf9dau, 0xffbda735u);
}

void test_graph_murmur3__v2_high_bit_short_tail(void)
{
	static const char data[] = "\x80";
	assert_v2(data, sizeof(data) - 1, 0x06cff4c0u, 0x081434bcu);
}

/*
 * Once a byte >= 0x80 is involved, v1 and v2 must disagree, at the very least
  * for platforms where char is signed. Lets test for that clash  to ensure 
  * bloom_filter_hash_version == 1 works
 */
void test_graph_murmur3__v1_and_v2_diverge_on_high_bit_input(void)
{
	static const char data[] = "\x80";
	cl_assert(git__hash(data, sizeof(data) - 1, SEED1) !=
		git__hash_v2(data, sizeof(data) - 1, SEED1));
}
