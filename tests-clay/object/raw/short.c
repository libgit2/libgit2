
#include "clay_libgit2.h"

#include "odb.h"
#include "hash.h"

void test_object_raw_short__oid_shortener_no_duplicates(void)
{
	git_oid_shorten *os;
	int min_len;

	os = git_oid_shorten_new(0);
	cl_assert(os != NULL);

	git_oid_shorten_add(os, "22596363b3de40b06f981fb85d82312e8c0ed511");
	git_oid_shorten_add(os, "ce08fe4884650f067bd5703b6a59a8b3b3c99a09");
	git_oid_shorten_add(os, "16a0123456789abcdef4b775213c23a8bd74f5e0");
	min_len = git_oid_shorten_add(os, "ce08fe4884650f067bd5703b6a59a8b3b3c99a09");

	cl_assert(min_len == GIT_OID_HEXSZ + 1);

	git_oid_shorten_free(os);
}

void test_object_raw_short__oid_shortener_stresstest_git_oid_shorten(void)
{
#define MAX_OIDS 1000

	git_oid_shorten *os;
	char *oids[MAX_OIDS];
	char number_buffer[16];
	git_oid oid;
	size_t i, j;

	int min_len = 0, found_collision;

	os = git_oid_shorten_new(0);
	cl_assert(os != NULL);

	/*
	 * Insert in the shortener 1000 unique SHA1 ids
	 */
	for (i = 0; i < MAX_OIDS; ++i) {
		char *oid_text;

		sprintf(number_buffer, "%u", (unsigned int)i);
		git_hash_buf(&oid, number_buffer, strlen(number_buffer));

		oid_text = git__malloc(GIT_OID_HEXSZ + 1);
		git_oid_fmt(oid_text, &oid);
		oid_text[GIT_OID_HEXSZ] = 0;

		min_len = git_oid_shorten_add(os, oid_text);
		cl_assert(min_len >= 0);

		oids[i] = oid_text;
	}

	/*
	 * Compare the first `min_char - 1` characters of each
	 * SHA1 OID. If the minimizer worked, we should find at
	 * least one collision
	 */
	found_collision = 0;
	for (i = 0; i < MAX_OIDS; ++i) {
		for (j = 0; j < MAX_OIDS; ++j) {
			if (i != j && memcmp(oids[i], oids[j], min_len - 1) == 0)
				found_collision = 1;
		}
	}
	cl_assert(found_collision == 1);

	/*
	 * Compare the first `min_char` characters of each
	 * SHA1 OID. If the minimizer worked, every single preffix
	 * should be unique.
	 */
	found_collision = 0;
	for (i = 0; i < MAX_OIDS; ++i) {
		for (j = 0; j < MAX_OIDS; ++j) {
			if (i != j && memcmp(oids[i], oids[j], min_len) == 0)
				found_collision = 1;
		}
	}
	cl_assert(found_collision == 0);

	/* cleanup */
	for (i = 0; i < MAX_OIDS; ++i)
		git__free(oids[i]);

	git_oid_shorten_free(os);

#undef MAX_OIDS
}
