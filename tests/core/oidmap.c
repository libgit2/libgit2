#include "clar_libgit2.h"
#include "oidmap.h"

typedef struct {
	git_oid oid;
	size_t extra;
} oidmap_item;

#define NITEMS 0x0fff

void test_core_oidmap__basic(void)
{
	git_oidmap *map;
	oidmap_item items[NITEMS];
	uint32_t i, j;

	for (i = 0; i < NITEMS; ++i) {
		items[i].extra = i;
		for (j = 0; j < GIT_OID_RAWSZ / 4; ++j) {
			items[i].oid.id[j * 4    ] = (unsigned char)i;
			items[i].oid.id[j * 4 + 1] = (unsigned char)(i >> 8);
			items[i].oid.id[j * 4 + 2] = (unsigned char)(i >> 16);
			items[i].oid.id[j * 4 + 3] = (unsigned char)(i >> 24);
		}
	}

	cl_git_pass(git_oidmap_new(&map));

	for (i = 0; i < NITEMS; ++i) {
		size_t pos;
		int ret;

		pos = git_oidmap_lookup_index(map, &items[i].oid);
		cl_assert(!git_oidmap_valid_index(map, pos));

		pos = git_oidmap_put(map, &items[i].oid, &ret);
		cl_assert(ret != 0);

		git_oidmap_set_value_at(map, pos, &items[i]);
	}


	for (i = 0; i < NITEMS; ++i) {
		size_t pos;

		pos = git_oidmap_lookup_index(map, &items[i].oid);
		cl_assert(git_oidmap_valid_index(map, pos));

		cl_assert_equal_p(git_oidmap_value_at(map, pos), &items[i]);
	}

	git_oidmap_free(map);
}

void test_core_oidmap__hash_collision(void)
{
	git_oidmap *map;
	oidmap_item items[NITEMS];
	uint32_t i, j;

	for (i = 0; i < NITEMS; ++i) {
		uint32_t segment = i / 8;
		int modi = i - (segment * 8);

		items[i].extra = i;

		for (j = 0; j < GIT_OID_RAWSZ / 4; ++j) {
			items[i].oid.id[j * 4    ] = (unsigned char)modi;
			items[i].oid.id[j * 4 + 1] = (unsigned char)(modi >> 8);
			items[i].oid.id[j * 4 + 2] = (unsigned char)(modi >> 16);
			items[i].oid.id[j * 4 + 3] = (unsigned char)(modi >> 24);
		}

		items[i].oid.id[ 8] = (unsigned char)i;
		items[i].oid.id[ 9] = (unsigned char)(i >> 8);
		items[i].oid.id[10] = (unsigned char)(i >> 16);
		items[i].oid.id[11] = (unsigned char)(i >> 24);
	}

	cl_git_pass(git_oidmap_new(&map));

	for (i = 0; i < NITEMS; ++i) {
		size_t pos;
		int ret;

		pos = git_oidmap_lookup_index(map, &items[i].oid);
		cl_assert(!git_oidmap_valid_index(map, pos));

		pos = git_oidmap_put(map, &items[i].oid, &ret);
		cl_assert(ret != 0);

		git_oidmap_set_value_at(map, pos, &items[i]);
	}


	for (i = 0; i < NITEMS; ++i) {
		size_t pos;

		pos = git_oidmap_lookup_index(map, &items[i].oid);
		cl_assert(git_oidmap_valid_index(map, pos));

		cl_assert_equal_p(git_oidmap_value_at(map, pos), &items[i]);
	}

	git_oidmap_free(map);
}

void test_core_oidmap__get_succeeds_with_existing_keys(void)
{
	git_oidmap *map;
	oidmap_item items[NITEMS];
	uint32_t i, j;

	for (i = 0; i < NITEMS; ++i) {
		uint32_t segment = i / 8;
		int modi = i - (segment * 8);

		items[i].extra = i;

		for (j = 0; j < GIT_OID_RAWSZ / 4; ++j) {
			items[i].oid.id[j * 4    ] = (unsigned char)modi;
			items[i].oid.id[j * 4 + 1] = (unsigned char)(modi >> 8);
			items[i].oid.id[j * 4 + 2] = (unsigned char)(modi >> 16);
			items[i].oid.id[j * 4 + 3] = (unsigned char)(modi >> 24);
		}

		items[i].oid.id[ 8] = (unsigned char)i;
		items[i].oid.id[ 9] = (unsigned char)(i >> 8);
		items[i].oid.id[10] = (unsigned char)(i >> 16);
		items[i].oid.id[11] = (unsigned char)(i >> 24);
	}

	cl_git_pass(git_oidmap_new(&map));

	for (i = 0; i < NITEMS; ++i) {
		int ret;
		git_oidmap_insert(map, &items[i].oid, &items[i], &ret);
		cl_assert(ret == 1);
	}

	for (i = 0; i < NITEMS; ++i)
		cl_assert_equal_p(git_oidmap_get(map, &items[i].oid), &items[i]);

	git_oidmap_free(map);
}

void test_core_oidmap__get_fails_with_nonexisting_key(void)
{
	git_oidmap *map;
	oidmap_item items[NITEMS];
	uint32_t i, j;

	for (i = 0; i < NITEMS; ++i) {
		uint32_t segment = i / 8;
		int modi = i - (segment * 8);

		items[i].extra = i;

		for (j = 0; j < GIT_OID_RAWSZ / 4; ++j) {
			items[i].oid.id[j * 4    ] = (unsigned char)modi;
			items[i].oid.id[j * 4 + 1] = (unsigned char)(modi >> 8);
			items[i].oid.id[j * 4 + 2] = (unsigned char)(modi >> 16);
			items[i].oid.id[j * 4 + 3] = (unsigned char)(modi >> 24);
		}

		items[i].oid.id[ 8] = (unsigned char)i;
		items[i].oid.id[ 9] = (unsigned char)(i >> 8);
		items[i].oid.id[10] = (unsigned char)(i >> 16);
		items[i].oid.id[11] = (unsigned char)(i >> 24);
	}

	cl_git_pass(git_oidmap_new(&map));

	/* Do _not_ add last OID to verify that we cannot look it up */
	for (i = 0; i < NITEMS - 1; ++i) {
		int ret;
		git_oidmap_insert(map, &items[i].oid, &items[i], &ret);
		cl_assert(ret == 1);
	}

	cl_assert_equal_p(git_oidmap_get(map, &items[NITEMS - 1].oid), NULL);

	git_oidmap_free(map);
}
