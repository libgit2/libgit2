/*
 * libgit2 "commit" example - shows how to create a git commit
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

typedef struct {
	char *message;
	int amend_head;
} commit_options;

/** Don't write an encoding header. Assume UTF-8. */
#define MESSAGE_ENCODING NULL

/// Returns 0 on success.
static int parse_options(commit_options *out, int argc, char **argv);

/**
 * This example demonstrates the libgit2 commit APIs to roughly
 * simulate `git commit` with the commit message argument.
 *
 * This does not have:
 *
 * - Robust error handling
 * - Most of the `git commit` options
 *
 * This does have:
 *
 * - Example of performing a git commit with a comment
 *
 */
int lg2_commit(git_repository *repo, int argc, char **argv)
{
	commit_options opts;
	int error;

	git_oid commit_oid,tree_oid;
	git_tree *tree = NULL;
	git_index *index = NULL;
	git_object *parent = NULL;
	git_reference *ref = NULL;
	git_commit *old_head = NULL;
	git_signature *signature = NULL;

	/* Validate args */
	if (parse_options(&opts, argc, argv)) {
		fprintf(stderr,
			"USAGE: %s [--amend] -m <comment>\n"
			"       %s [--amend] --message <comment>\n"
			"           Commit with message, <comment>.\n"
			"           If --amend is given, replace\n"
			"           HEAD with this commit.\n", argv[0], argv[0]);
		return -1;
	}

	error = git_revparse_ext(&parent, &ref, repo, "HEAD");
	if (error == GIT_ENOTFOUND) {
		error = 0;

		if (opts.amend_head) {
			fprintf(stderr, "HEAD not found. Unable to amend.\n");
			error = 1;
			goto cleanup;
		}

		printf("HEAD not found. Creating the first commit.\n");
	} else if (error != 0) {
		const git_error *err = git_error_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);

		goto cleanup;
	} else if (opts.amend_head) {
		error = get_repo_head(&old_head, repo);
		if (error) {
			goto cleanup;
		}

		error = git_commit_amend(&commit_oid,
				old_head,
				"HEAD", // Make HEAD point to the new commit.
				NULL, // Leave the author untouched
				NULL, // Leave the committer untouched
				MESSAGE_ENCODING,
				opts.message,
				NULL); // Use the same tree
		printf("Updated HEAD.\n");
		goto cleanup;
	}

	check_lg2(git_repository_index(&index, repo), "Could not open repository index", NULL);
	check_lg2(git_index_write_tree(&tree_oid, index), "Could not write tree", NULL);
	check_lg2(git_index_write(index), "Could not write index", NULL);

	check_lg2(git_tree_lookup(&tree, repo, &tree_oid), "Error looking up tree", NULL);

	check_lg2(git_signature_default(&signature, repo), "Error creating signature", NULL);

	check_lg2(git_commit_create_v(
		&commit_oid,
		repo,
		"HEAD",
		signature,
		signature,
		MESSAGE_ENCODING,
		opts.message,
		tree,
		parent ? 1 : 0, parent), "Error creating commit", NULL);

cleanup:
	git_index_free(index);
	git_signature_free(signature);
	git_commit_free(old_head);
	git_tree_free(tree);

	return error;
}

static int parse_options(commit_options *out, int argc, char **argv)
{
	int i;

	out->message = NULL;
	out->amend_head = 0;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (*arg != '-') {
			return 1;
		}

		if (strcmp(arg, "--amend") == 0) {
			out->amend_head = 1;
		} else if (strcmp(arg, "-m") == 0 || strcmp(arg, "--message") == 0) {
			i++;
			if (i == argc) {
				fprintf(stderr, "Missing required argument to --message.\n");
				return 1;
			}

			out->message = argv[i];
		} else {
			fprintf(stderr, "Unrecognised option: %s\n", arg);
			return 1;
		}
	}

	if (out->message == NULL) {
		fprintf(stderr,
			"At present, the --message argument is required. "
			"It was not given.\n");
		return 1;
	}

	return 0;
}

