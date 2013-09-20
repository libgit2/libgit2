#ifndef INCLUDE_blame_h__
#define INCLUDE_blame_h__

#include "git2/blame.h"
#include "common.h"
#include "vector.h"
#include "diff.h"
#include "array.h"
#include "git2/oid.h"

struct git_blame {
	const char *path;
	git_repository *repository;
	git_blame_options options;

	git_vector hunks;
	git_vector paths;

	git_blob *final_blob;

	size_t current_diff_line;
	git_blame_hunk *current_hunk;
};

git_blame *git_blame__alloc(
	git_repository *repo,
	git_blame_options opts,
	const char *path);

#endif
