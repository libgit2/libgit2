#ifndef INCLUDE_revwalk_pathspec_helpers_h__
#define INCLUDE_revwalk_pathspec_helpers_h__

#include "clar_libgit2.h"

/**
 * A single pathspec-walk scenario: push `pushref` (or HEAD, if NULL),
 * filter by a pathspec built from `patterns`, and expect exactly
 * `expected_oid_strs` back, in order.
 *
 * All scenarios here are independent of whether the repo's commit-graph
 * carries changed-path Bloom filters, so they're defined once and run
 * against both revwalk::pathspec (no Bloom data) and revwalk::pathspec::bloom
 * (real Bloom data) via pathspec_run_cases().
 */
typedef struct {
	const char *name;
	const char *pushref;
	char **patterns;
	size_t pattern_count;
	const char **expected_oid_strs;
	size_t expected_count;
} pathspec_case;

extern const pathspec_case pathspec_cases[];
extern const size_t pathspec_cases_count;

/* Shared expected-oid lists, also reused by lifecycle tests that only make
 * sense without Bloom filters (changing/clearing the pathspec mid-walk). */
extern const char *pathspec_expected_exact_str[2];
extern const char *pathspec_expected_all_str[7];

void pathspec_assert_walk(
	git_repository *repo,
	const char *name,
	const char *pushref,
	char **patterns,
	size_t pattern_count,
	const char **expected_oid_strs,
	size_t expected_count);

/* Runs every scenario in pathspec_cases against `repo`. */
void pathspec_run_cases(git_repository *repo);

#endif
