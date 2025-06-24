/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_bundle_h__
#define INCLUDE_bundle_h__

#include "common.h"
#include "vector.h"

#include "git2/indexer.h"
#include "git2/oid.h"
#include "git2/types.h"

typedef struct {
	int version;
	git_oid_t oid_type;
	git_vector prerequisites;
	git_vector refs;
} git_bundle_header;

int git_bundle_header_open(git_bundle_header **out, const char *url);
void git_bundle_header_free(git_bundle_header *bundle);

int git_bundle__is_bundle(const char *url);

int git_bundle__read_pack(
        git_repository *repo,
        const char *url,
        git_indexer_progress *stats);

#endif
