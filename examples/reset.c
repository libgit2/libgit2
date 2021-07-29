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
	git_reset_t reset_type;
} reset_opts;

static int parse_options(reset_opts *out, struct args_info *args, git_repository *repo);

int lg2_reset(git_repository *repo, int argc, char **argv)
{
	reset_opts options;
	git_object *target = NULL;
	struct args_info args = ARGS_INFO_INIT;
	int error = 0;

	if (parse_options(&options, &args, repo)) {
		fprintf(stderr,
			"USAGE: %s [<treeish>] [--hard] [--] [<pathspec>...]\n"
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
	 * checkout_opts can be used to track reset progress for a hard reset.
	 */
	if (options.paths_to_reset.count == 0 || options.reset_type == GIT_RESET_HARD) {
		git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

		if (options.paths_to_reset.count > 0) {
			checkout_opts.paths = options.paths_to_reset;
		}

		error = git_reset(repo, target, options.reset_type, &checkout_opts);
	} else {
		/**
		 * If we just have a list of paths to reset, we can use `git_reset_default`.
		 */

		if (options.reset_type != GIT_RESET_SOFT) {
			fprintf(stderr, "WARNING: Non-soft reset: Not supported for a list of paths.\n");
		}

		error = git_reset_default(repo, target, &options.paths_to_reset);
	}

cleanup:
	git_object_free(target);

	return error;
}

static int parse_options(reset_opts *out, struct args_info *args, git_repository *repo)
{
	int i;
	int path_start = -1;
	int argc = args->argc;
	char **argv = args->argv;

	out->reset_to = "HEAD"; // Default to "HEAD" for what to reset to.
	out->paths_to_reset.count = 0;
	out->paths_to_reset.strings = NULL;
	out->reset_type = GIT_RESET_SOFT;

	if (argc == 1) {
		// We need to have at least one argument.
		return 1;
	}

	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-') {
			out->reset_to = argv[i];

			// Start processing subsequent arguments as paths.
			i++;
			break;
		} else if (strcmp(argv[i], "--hard") == 0) {
			out->reset_type = GIT_RESET_HARD;
		} else if (strcmp(argv[i], "--") == 0) {
			// After --, all arguments are interpreted as paths.
			i++;
			break;
		} else {
			// Display help.
			return 1;
		}
	}

	// We're now parsing pathspecs, if there are any.
	path_start = i;
	args->pos = i;
	out->paths_to_reset.count = argc - path_start;

	if (out->paths_to_reset.count > 0) {
		out->paths_to_reset.strings = (char **) malloc(out->paths_to_reset.count * sizeof(char*));

		// Make all given paths relative to the repo's working directory.
		for (i = path_start; i < argc; i++) {
			get_repopath_to(&out->paths_to_reset.strings[i - path_start], argv[i], repo);
		}
	}

	return 0;
}

