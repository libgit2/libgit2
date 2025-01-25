/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_blame_h__
#define INCLUDE_blame_h__

#include "common.h"
#include "array.h"
#include "hashmap_oid.h"

GIT_HASHMAP_OID_STRUCT(git_blame_contributormap, git_commit *);

typedef struct {
	const char *contents;
	size_t contents_len;
	git_commit *commit;
	unsigned int definitive;
} git_blame_line_candidate;

struct git_blame {
	git_repository *repository;
	git_blame_options options;

	char *path;

	/*
	 * The contents of the final file (either the "newest" blob)
	 * or the contents of the working directory file. The contents
	 * pointer points to either the contents_buf or the contents_blob.
	 */
	const char *contents;
	size_t contents_len;

	git_array_t(git_blame_line_candidate) lines;

	git_str contents_buf;
	git_blob *contents_blob;

	git_revwalk *revwalk;
	git_blame_contributormap contributors;

	git_commit *current_commit;
};

#endif
