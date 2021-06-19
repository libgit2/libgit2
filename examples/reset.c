/*
 * libgit2 "reset" example - shows how reset files and/or repository state
 *
 * Written by libgit2 and a-Shell contributors
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */
#include "common.h"

/**
 * This example demonstrates usage of the libgit2 resetting APIs to
 * roughly simulate `git reset`.
 *
 * This does not have:
 *
 * - Robust error handling
 * - Most of the `git reset` options
 *
 */

typedef struct {
	char *reset_to;
	git_strarray paths_to_reset;
} reset_opts;

static int parse_options(reset_opts *out, int argc, char **argv);

int lg2_reset(git_repository *repo, int argc, char **argv)
{
	reset_opts options;
	git_object *target = NULL;
	int error = 0;

	if (parse_options(&options, argc, argv)) {
		fprintf(stderr,
			"USAGE: %s [<treeish>] [--] [<pathspec>...]\n"
			"    <treeish>:  Where to reset to. Defaults to HEAD."
					" At present, only soft resets are supported.\n"
			"    <pathspec>: If any are given, rather than resetting"
			" the entire repository, just reset each given path."
			" Note that if a given path does not exist, this command,"
			" regardless, exits successfully, without warning or error.\n", argv[0]);
		error = -1;

		goto cleanup;
	}

	/**
	 * Is `options.reset_to` something like `HEAD`, a tag, something else?
	 * git_revparse_single looks it up and gives us a git_object.
	 */
	error = git_revparse_single(&target, repo, options.reset_to);
	if (error != 0) {
		fprintf(stderr,
			"Error looking up target. `%s' isn't a commit or a tag!\n",
			options.reset_to);
		goto cleanup;
	}

	/**
	 * Reset the entire repository. Here, we're doing a SOFT reset.
	 * See the git_reset_t enum for additional options.
	 *
	 * We're not doing a GIT_RESET_HARD and not tracking progress,
	 * so checkout_opts are null.
	 */
	if (options.paths_to_reset.count == 0) {
		error = git_reset(repo, target, GIT_RESET_SOFT, NULL);
	} else {
		error = git_reset_default(repo, target, &options.paths_to_reset);
	}

cleanup:
	git_object_free(target);

	return error;
}

/// Lifetime(out) <= Lifetime(argv)
static int parse_options(reset_opts *out, int argc, char **argv)
{
	int i;
	int path_start = -1;

	out->reset_to = NULL;
	out->paths_to_reset.count = 0;

	if (argc == 1) {
		// We need to have at least one argument.
		return 1;
	}

	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-') {
			if (i >= 2) {
				// reset --somearg1 --somearg2 path1 path2...
				//                              ^^^
				//                               i

				// We did't have a tree-like reset_to argument.
				// We default to HEAD.
				out->reset_to = "HEAD";

				break;
			}

			// Otherwise, it's the <tree-like> argument.
			out->reset_to = argv[i];
		} else if (strcmp(argv[i], "--") == 0) {
			// Start processing path arguments.
			break;
		} else {
			// Display help.
			return 1;
		}
	}

	// We're now parsing pathspecs, if there are any.
	path_start = i;

	out->paths_to_reset.count = argc - path_start;
	out->paths_to_reset.strings = path_start < argc ? argv + path_start : NULL;

	return 0;
}

