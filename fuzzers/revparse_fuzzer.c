/*
 * libgit2 revparse fuzzer target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <string.h>

#include "git2.h"

#include "standalone_driver.h"
#include "fuzzer_utils.h"

#define UNUSED(x) (void)(x)

static git_repository *repo;

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	UNUSED(argc);
	UNUSED(argv);

	if (git_libgit2_init() < 0)
		abort();

	if (git_libgit2_opts(GIT_OPT_SET_PACK_MAX_OBJECTS, 10000000) < 0)
		abort();

	repo = fuzzer_repo_init();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	git_object *obj = NULL;
	char *c;

	if ((c = calloc(1, size + 1)) == NULL)
		abort();

	memcpy(c, data, size);

	git_revparse_single(&obj, repo, c);
	git_object_free(obj);
	free(c);

	return 0;
}
