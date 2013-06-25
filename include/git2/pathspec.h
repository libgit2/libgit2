/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_pathspec_h__
#define INCLUDE_git_pathspec_h__

#include "common.h"
#include "types.h"
#include "strarray.h"

/**
 * Compiled pathspec
 */
typedef struct git_pathspec git_pathspec;

/**
 * List of filenames matching a pathspec
 */
typedef struct git_pathspec_match_list git_pathspec_match_list;

/**
 * Options controlling how pathspec match should be executed
 *
 * - GIT_PATHSPEC_IGNORE_CASE forces match to ignore case; otherwise
 *   match will use native case sensitivity of platform
 * - GIT_PATHSPEC_USE_CASE forces case sensitive match; otherwise
 *   match will use native case sensitivity of platform
 * - GIT_PATHSPEC_NO_GLOB disables glob patterns and just uses simple
 *   string comparison for matching
 * - GIT_PATHSPEC_NO_MATCH_ERROR means the match function will return
 *   GIT_ENOTFOUND if no matches are found; otherwise it will return 0
 *   for success and `git_pathspec_match_list_entrycount` will be 0.
 * - GIT_PATHSPEC_FIND_FAILURES only applies to a git_pathspec_match_list;
 *   it means to check file names against all unmatched patterns so that
 *   at the end of a match we can identify patterns that did not match any
 *   files.
 * - GIT_PATHSPEC_FAILURES_ONLY only applies to a git_pathspec_match_list;
 *   it means to only check for mismatches and not record matched paths.
 */
typedef enum {
	GIT_PATHSPEC_DEFAULT        = 0,
	GIT_PATHSPEC_IGNORE_CASE    = (1u << 0),
	GIT_PATHSPEC_USE_CASE       = (1u << 1),
	GIT_PATHSPEC_NO_GLOB        = (1u << 2),
	GIT_PATHSPEC_NO_MATCH_ERROR = (1u << 3),
	GIT_PATHSPEC_FIND_FAILURES  = (1u << 4),
	GIT_PATHSPEC_FAILURES_ONLY  = (1u << 5),
} git_pathspec_flag_t;

/**
 * Compile a pathspec
 *
 * @param out Output of the compiled pathspec
 * @param flags Combination of git_pathspec_flag_t values
 * @param pathspec A git_strarray of the paths to match
 * @return 0 on success, <0 on failure
 */
GIT_EXTERN(int) git_pathspec_new(
	git_pathspec **out, const git_strarray *pathspec);

/**
 * Free a pathspec
 *
 * @param ps The compiled pathspec
 */
GIT_EXTERN(void) git_pathspec_free(git_pathspec *ps);

/**
 * Try to match a path against a pathspec
 *
 * Unlike most of the other pathspec matching functions, this will not
 * fall back on the native case-sensitivity for your platform.  You must
 * explicitly pass flags to control case sensitivity or else this will
 * fall back on being case sensitive.
 *
 * @param ps The compiled pathspec
 * @param flags Match flags to influence matching behavior
 * @param path The pathname to attempt to match
 * @return 1 is path matches spec, 0 if it does not
 */
GIT_EXTERN(int) git_pathspec_matches_path(
	const git_pathspec *ps, uint32_t flags, const char *path);

/**
 * Match a pathspec against the working directory of a repository.
 *
 * This returns a `git_patchspec_match` object that contains the list of
 * all files matching the given pathspec in the working directory of the
 * repository.  This handles git ignores (i.e. ignored files will not be
 * considered to match the `pathspec` unless the file is tracked in the
 * index).
 *
 * @param out Object with list of matching items
 * @param repo The repository in which to match; bare repo is an error
 * @param flags Options to control matching behavior
 * @param ps Pathspec to be matched
 * @return 0 on success, -1 on error, GIT_ENOTFOUND if no matches and
 *         the GIT_PATHSPEC_NO_MATCH_ERROR flag is used
 */
GIT_EXTERN(int) git_pathspec_match_workdir(
	git_pathspec_match_list **out,
	git_repository *repo,
	uint32_t flags,
	git_pathspec *ps);

/**
 * Match a pathspec against entries in an index.
 *
 * This returns a `git_patchspec_match` object that contains the list of
 * all files matching the given pathspec in the index.
 *
 * NOTE: At the moment, the case sensitivity of this match is controlled
 * by the current case-sensitivity of the index object itself and the
 * USE_CASE and IGNORE_CASE flags will have no effect.  This behavior will
 * be corrected in a future release.
 *
 * @param out Object with list of matching items
 * @param inex The index in which to match
 * @param flags Options to control matching behavior
 * @param ps Pathspec to be matched
 * @return 0 on success, -1 on error, GIT_ENOTFOUND if no matches and
 *         the GIT_PATHSPEC_NO_MATCH_ERROR flag is used
 */
GIT_EXTERN(int) git_pathspec_match_index(
	git_pathspec_match_list **out,
	git_index *index,
	uint32_t flags,
	git_pathspec *ps);

/**
 * Match a pathspec against files in a tree.
 *
 * This returns a `git_patchspec_match` object that contains the list of
 * all files matching the given pathspec in the given tree.
 *
 * @param out Object with list of matching items
 * @param inex The index in which to match
 * @param flags Options to control matching behavior
 * @param ps Pathspec to be matched
 * @return 0 on success, -1 on error, GIT_ENOTFOUND if no matches and
 *         the GIT_PATHSPEC_NO_MATCH_ERROR flag is used
 */
GIT_EXTERN(int) git_pathspec_match_tree(
	git_pathspec_match_list **out,
	git_tree *tree,
	uint32_t flags,
	git_pathspec *ps);

/**
 * Free memory associates with a git_pathspec_match_list
 *
 * @param m The git_pathspec_match_list to be freed
 */
GIT_EXTERN(void) git_pathspec_match_list_free(git_pathspec_match_list *m);

/**
 * Get the number of items in a match list.
 *
 * @param m The git_pathspec_match_list object
 * @return Number of items in match list
 */
GIT_EXTERN(size_t) git_pathspec_match_list_entrycount(
	const git_pathspec_match_list *m);

/**
 * Get a matching filename by position.
 *
 * @param m The git_pathspec_match_list object
 * @param pos The index into the list
 * @return The filename of the match
 */
GIT_EXTERN(const char *) git_pathspec_match_list_entry(
	const git_pathspec_match_list *m, size_t pos);

/**
 * Get the number of pathspec items that did not match.
 *
 * This will be zero unless you passed GIT_PATHSPEC_FIND_FAILURES when
 * generating the git_pathspec_match_list.
 *
 * @param m The git_pathspec_match_list object
 * @return Number of items in original pathspec that had no matches
 */
GIT_EXTERN(size_t) git_pathspec_match_list_failed_entrycount(
	const git_pathspec_match_list *m);

/**
 * Get an original pathspec string that had no matches.
 *
 * This will be return NULL for positions out of range.
 *
 * @param m The git_pathspec_match_list object
 * @param pos The index into the failed items
 * @return The pathspec pattern that didn't match anything
 */
GIT_EXTERN(const char *) git_pathspec_match_list_failed_entry(
	const git_pathspec_match_list *m, size_t pos);

#endif
