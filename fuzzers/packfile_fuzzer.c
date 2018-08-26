/*
 * libgit2 packfile fuzzer target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#include "git2.h"
#include "git2/sys/mempack.h"

#define UNUSED(x) (void)(x)

static git_odb *odb = NULL;
static git_odb_backend *mempack = NULL;

/* Arbitrary object to seed the ODB. */
static const unsigned char base_obj[] = { 07, 076 };
static const unsigned int base_obj_len = 2;

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	UNUSED(argc);
	UNUSED(argv);
	if (git_libgit2_init() < 0) {
		fprintf(stderr, "Failed to initialize libgit2\n");
		abort();
	}
	if (git_libgit2_opts(GIT_OPT_SET_PACK_MAX_OBJECTS, 10000000) < 0) {
		fprintf(stderr, "Failed to limit maximum pack object count\n");
		abort();
	}
	if (git_odb_new(&odb) < 0) {
		fprintf(stderr, "Failed to create the odb\n");
		abort();
	}
	if (git_mempack_new(&mempack) < 0) {
		fprintf(stderr, "Failed to create the mempack\n");
		abort();
	}
	if (git_odb_add_backend(odb, mempack, 999) < 0) {
		fprintf(stderr, "Failed to add the mempack\n");
		abort();
	}
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	git_indexer *indexer = NULL;
	git_transfer_progress stats = {0, 0};
	bool append_hash = false;
	git_oid id;
	char hash[GIT_OID_HEXSZ + 1] = {0};
	char path[PATH_MAX];

	if (size == 0)
		return 0;

	if (!odb || !mempack) {
		fprintf(stderr, "Global state not initialized\n");
		abort();
	}
	git_mempack_reset(mempack);

	if (git_odb_write(&id, odb, base_obj, base_obj_len, GIT_OBJ_BLOB) < 0) {
		fprintf(stderr, "Failed to add an object to the odb\n");
		abort();
	}

	if (git_indexer_new(&indexer, ".", 0, odb, NULL) < 0) {
		fprintf(stderr, "Failed to create the indexer: %s\n",
			giterr_last()->message);
		abort();
	}

	/*
	 * If the first byte in the stream has the high bit set, append the
	 * SHA1 hash so that the packfile is somewhat valid.
	 */
	append_hash = *data & 0x80;
	++data;
	--size;

	if (git_indexer_append(indexer, data, size, &stats) < 0)
		goto cleanup;
	if (append_hash) {
		git_oid oid;
		if (git_odb_hash(&oid, data, size, GIT_OBJ_BLOB) < 0) {
			fprintf(stderr, "Failed to compute the SHA1 hash\n");
			abort();
		}
		if (git_indexer_append(indexer, &oid, sizeof(oid), &stats) < 0) {
			goto cleanup;
		}
	}
	if (git_indexer_commit(indexer, &stats) < 0)
		goto cleanup;

	/*
	 * We made it! We managed to produce a valid packfile.
	 * Let's clean it up.
	 */
	git_oid_fmt(hash, git_indexer_hash(indexer));
	printf("Generated packfile %s\n", hash);
	snprintf(path, sizeof(path), "pack-%s.idx", hash);
	unlink(path);
	snprintf(path, sizeof(path), "pack-%s.pack", hash);
	unlink(path);

cleanup:
	git_mempack_reset(mempack);
	git_indexer_free(indexer);
	return 0;
}
