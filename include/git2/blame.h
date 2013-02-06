/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_git_blame_h__
#define INCLUDE_git_blame_h__

#include "common.h"
#include "oid.h"

/**
 * @file git2/blame.h
 * @brief Git blame routines
 * @defgroup git_blame Git blame routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Flags for indicating option behavior for git_blame APIs.
 */
typedef enum {
	/** Normal blame, the default */
	GIT_BLAME_NORMAL = 0,
	/** Track lines that have moved within a file (like `git blame -M`) */
	GIT_BLAME_TRACK_COPIES_SAME_FILE = (1<<0),
	/** Track lines that have moved across files in the same commit (like `git blame -C`) */
	GIT_BLAME_TRACK_COPIES_SAME_COMMIT_MOVES = (1<<1),
	/** Track lines that have been copied from another file that exists in the
	 * same commit (like `git blame -CC`) */
	GIT_BLAME_TRACK_COPIES_SAME_COMMIT_COPIES = (1<<1 | 1<<2),
	/** Track lines that have been copied from another file that exists in *any*
	 * commit (like `git blame -CCC`) */
	GIT_BLAME_TRACK_COPIES_ANY_COMMIT_COPIES = (1<<1 | 1<<2 | 1<<3),
} git_blame_flag_t;

/**
 * Blame options structure
 *
 * Use zeros to indicate default settings.  It's easiest to use the
 * `GIT_BLAME_OPTIONS_INIT` macro:
 *     git_blame_options opts = GIT_BLAME_OPTIONS_INIT;
 *
 * - `flags` is a combination fo teh `git_blame_flag_t` values above.
 * - `min_match_characters` is the lower bound on the number of alphanumeric
 *   characters that must be detected as moving/copying within a file for it to
 *   associate those lines with the parent commit. The default value is 20.
 *   This value only takes effect if any of the `GIT_BLAME_TRACK_COPIES_*`
 *   flags are specified.
 * - `newest_commit` is a rev-parse spec that resolves to the most recent
 *   commit to consider.  The default is HEAD.
 * - `newest_commit` is the newest commit to consider.  The default is HEAD.
 * - `oldest_commit` is the oldest commit to consider.  The default is the
 *   first commit encountered with a NULL parent.
 *	- `min_line` is the first line in the file to blame.  The default is 1 (line
 *	  numbers start with 1).
 *	- `max_line` is the last line in the file to blame.  The default is the last
 *	  line of the file.
 */

typedef struct git_blame_options {
	unsigned int version;

	uint32_t flags;
	uint16_t min_match_characters;
	const git_commit *newest_commit;
	const git_commit *oldest_commit;
	uint32_t min_line;
	uint32_t max_line;
} git_blame_options;

#define GIT_BLAME_OPTIONS_VERSION 1
#define GIT_BLAME_OPTIONS_INIT {GIT_BLAME_OPTIONS_VERSION}

/**
 * Structure that represents a blame hunk.
 *
 * - `lines_in_hunk` is the number of lines in this hunk
 * - `final_commit_id` is the OID of the commit where this line was last
 *   changed.
 * - `final_start_line_number` is the 1-based line number where this hunk
 *   begins, in the final version of the file
 * - `orig_commit_id` is the OID of the commit where this hunk was found.  This
 *   will usually be the same as `final_commit_id`, except when
 *   `GIT_BLAME_TRACK_COPIES_ANY_COMMIT_COPIES` has been specified.
 * - `orig_path` is the path to the file where this hunk originated, as of the
 *   commit specified by `orig_commit_id`.
 * - `orig_start_line_number` is the 1-based line number where this hunk begins
 *   in the file named by `orig_path` in the commit specified by
 *   `orig_commit_id`.
 */
typedef struct git_blame_hunk {
	uint16_t lines_in_hunk;

	git_oid final_commit_id;
	uint16_t final_start_line_number;

	git_oid orig_commit_id;
	const char *orig_path;
	uint16_t orig_start_line_number;
} git_blame_hunk;


typedef struct git_blame_results git_blame_results;

/**
 * Gets the number of hunks that exist in the results structure.
 */
GIT_EXTERN(uint32_t) git_blame_results_hunk_count(git_blame_results *results);

/**
 * Gets the blame hunk at the given index.
 *
 * @param results the results structure to query
 * @param index index of the hunk to retrieve
 * @return the hunk at the given index, or NULL on error
 */
GIT_EXTERN(const git_blame_hunk*) git_blame_results_hunk_byindex(
		git_blame_results *results,
		uint32_t index);

/**
 * Gets the hunk that relates to the given line number in the newest commit.
 *
 * @param results the results structure to query
 * @param lineno the (1-based) line number to find a hunk for
 * @return the hunk that contains the given line, or NULL on error
 */
GIT_EXTERN(const git_blame_hunk*) git_blame_results_hunk_byline(
		git_blame_results *results,
		uint32_t lineno);

/**
 * Get the blame for a single file.
 *
 * @param out pointer that will receive the results object
 * @param repo repository whose history is to be walked
 * @param path path to file to consider
 * @param options options for the blame operation.  If NULL, this is treated as
 *                though GIT_BLAME_OPTIONS_INIT were passed.
 * @return 0 on success, or an error code. (use giterr_last for information
 *         about the error.)
 */
GIT_EXTERN(int) git_blame_file(
		git_blame_results **out,
		git_repository *repo,
		const char *path,
		git_blame_options *options);


/**
 * Free memory allocated by git_blame.
 *
 * @param results results structure to free
 */
GIT_EXTERN(void) git_blame_free(git_blame_results *results);

/** @} */
GIT_END_DECL
#endif

