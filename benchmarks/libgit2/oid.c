#include "clar.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <git2.h>

#define BENCHMARK_OID_COUNT 256

static git_oid sha1_one[BENCHMARK_OID_COUNT];
static git_oid *sha1_two;

#ifdef GIT_EXPERIMENTAL_SHA256
static git_oid sha256_one[BENCHMARK_OID_COUNT];
static git_oid *sha256_two;
#endif

static void update_data_to_val(git_oid *out, git_oid_t type, uint32_t val)
{
	unsigned char data[GIT_OID_MAX_SIZE] = {0};
	size_t size;

#ifdef GIT_EXPERIMENTAL_SHA256
	size = (type == GIT_OID_SHA256) ? GIT_OID_SHA256_SIZE : GIT_OID_SHA1_SIZE;
#else
	size = GIT_OID_SHA1_SIZE;

	((void)(type));
#endif

	memset(data, 0, GIT_OID_MAX_SIZE);

	data[size - 1] = (unsigned char)(val & 0x000000ff);
	data[size - 2] = (unsigned char)((val & 0x0000ff00) >> 8);
	data[size - 3] = (unsigned char)((val & 0x00ff0000) >> 16);
	data[size - 4] = (unsigned char)((val & 0x00ff0000) >> 24);

#ifdef GIT_EXPERIMENTAL_SHA256
	cl_assert(git_oid_from_raw(out, data, type) == 0);
#else
	cl_assert(git_oid_fromraw(out, data) == 0);
#endif
}

void benchmark_oid__initialize(void)
{
	uint32_t accum = 0;
	size_t i;

	sha1_two = calloc(BENCHMARK_OID_COUNT, sizeof(git_oid));
	cl_assert(sha1_two != NULL);

#ifdef GIT_EXPERIMENTAL_SHA256
	sha256_two = calloc(BENCHMARK_OID_COUNT, sizeof(git_oid));
	cl_assert(sha256_two != NULL);
#endif

	for (i = 0; i < BENCHMARK_OID_COUNT; i++) {
		update_data_to_val(&sha1_one[i], GIT_OID_SHA1, accum++);
		update_data_to_val(&sha1_two[i], GIT_OID_SHA1, accum++);
	}

#ifdef GIT_EXPERIMENTAL_SHA256
	for (i = 0; i < BENCHMARK_OID_COUNT; i++) {
		update_data_to_val(&sha256_one[i], GIT_OID_SHA256, accum++);
		update_data_to_val(&sha256_two[i], GIT_OID_SHA256, accum++);
	}
#endif
}

void benchmark_oid__reset(void)
{
}

void benchmark_oid__cleanup(void)
{
	free(sha1_two);

#ifdef GIT_EXPERIMENTAL_SHA256
	free(sha256_two);
#endif
}

void benchmark_oid__cmp_sha1(void)
{
	size_t i, j;

	for (i = 0; i < 1024 * 16; i++)
		for (j = 0; j < BENCHMARK_OID_COUNT; j++)
			git_oid_cmp(&sha1_one[j], &sha1_two[j]);
}

void benchmark_oid__cmp_sha256(void)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	size_t i, j;

	for (i = 0; i < 1024 * 16; i++)
		for (j = 0; j < BENCHMARK_OID_COUNT; j++)
			git_oid_cmp(&sha256_one[j], &sha256_two[j]);
#else
	clar__skip();
#endif
}

void benchmark_oid__cpy_sha1(void)
{
	git_oid dest;
	size_t i, j;

	for (i = 0; i < 1024 * 16; i++)
		for (j = 0; j < BENCHMARK_OID_COUNT; j++)
			git_oid_cpy(&dest, &sha1_one[j]);
}

void benchmark_oid__cpy_sha256(void)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	git_oid dest;
	size_t i, j;

	for (i = 0; i < 1024 * 16; i++)
		for (j = 0; j < BENCHMARK_OID_COUNT; j++)
			git_oid_cpy(&dest, &sha256_one[j]);
#else
	clar__skip();
#endif
}

void benchmark_oid__zero_sha1(void)
{
	size_t i, j;

	for (i = 0; i < 1024 * 16; i++)
		for (j = 0; j < BENCHMARK_OID_COUNT; j++)
			git_oid_is_zero(&sha1_one[j]);
}

void benchmark_oid__zero_sha256(void)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	size_t i, j;

	for (i = 0; i < 1024 * 16; i++)
		for (j = 0; j < BENCHMARK_OID_COUNT; j++)
			git_oid_is_zero(&sha256_one[j]);
#else
	clar__skip();
#endif
}
