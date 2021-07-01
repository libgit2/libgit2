/*
 * libgit2 "apply" example - shows how to apply diffs
 *
 * Written by the libgit2 contributors
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
 * This example demonstrates the libgit2 push API to roughly
 * simulate `git apply`.
 *
 * This does not have:
 *
 * - Robust error handling
 * - Most of the `git apply` options
 *
 * This does have:
 *
 * - Example of applying a patch from a file.
 *
 */

int lg2_apply(git_repository *repo, int argc, char **argv)
{
	int error = 0;
	git_diff *diff = NULL;
	char *patch_contents = NULL;

	if (argc != 2) {
		printf("Usage: %s <path_to_patch_file>\n", argv[0]);
		exit(1);
	}

	patch_contents = read_file(argv[1]);
	error = git_diff_from_buffer(&diff, patch_contents, strlen(patch_contents));
	if (error) {
		printf("Error converting diff to buffer.\n");
		goto cleanup;
	}

	error = git_apply(repo, diff, GIT_APPLY_LOCATION_WORKDIR, NULL);

cleanup:
	free(patch_contents);
	git_diff_free(diff);

	return error;
}

