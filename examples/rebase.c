/*
 * libgit2 "rebase" example - shows how to perform merges
 *
 * Written by the libgit2 and a-Shell contributors
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
 * This example roughly simulates the `git rebase` command.
 */

typedef struct {
	git_annotated_commit *upstream;
	git_annotated_commit *onto;
	int stop_existing;
	int continue_existing;
} rebase_options;

static int start_rebase(rebase_options *options, git_repository *repo);
static int continue_rebase(git_rebase *rebase, git_repository *repo);
static int abort_rebase(git_rebase *rebase);

static int parse_args(rebase_options *options, git_repository *repo, int argc, char **argv);
static void free_rebase_options(rebase_options *options);

int lg2_rebase(git_repository *repo, int argc, char **argv)
{
	int error = 0;
	rebase_options opts;

	if (parse_args(&opts, repo, argc, argv)) {
		fprintf(stderr, "USAGE: %s <upstream>\n", argv[0]);
		fprintf(stderr, "           Makes <upstream> part of this branch's history.\n");
		fprintf(stderr, "       %s [--abort|--continue]\n", argv[0]);
		fprintf(stderr, "           --abort: Cancels an existing rebase.\n");
		fprintf(stderr, "           --continue: Continues a paused rebase.\n");
		error = -1;
		goto cleanup;
	}

	if (!opts.stop_existing && !opts.continue_existing) {
		// We don't need to open an existing rebase to start
		// a new rebase.
		start_rebase(&opts, repo);
	} else {
		git_rebase *rebase = NULL;
		error = git_rebase_open(&rebase,
			repo,
			NULL); // No additional options.
		if (error) {
			fprintf(stderr, "Unable to open an existing rebase!\n");
			goto cleanup;
		}

		if (opts.stop_existing) {
			error = abort_rebase(rebase);
		} else if (opts.continue_existing) {
			error = continue_rebase(rebase, repo);
		}

		git_rebase_free(rebase);
	}

cleanup:
	free_rebase_options(&opts);
	return error;
}

static int start_rebase(rebase_options *options, git_repository *repo)
{
	int error = 0;
	git_rebase *rebase = NULL;

	error = git_rebase_init(&rebase,
		repo,
		NULL, // Rebase the current branch
		options->upstream,
		options->onto,
		NULL); // No additional options.

	if (error != 0) {
		fprintf(stderr, "Error initializing rebase!\n");
		goto cleanup;
	}

	error = continue_rebase(rebase, repo);

cleanup:
	git_rebase_free(rebase);
	return error;
}

static int continue_rebase(git_rebase *rebase, git_repository *repo)
{
	int error = 0;
	size_t operation_index = 0;
	git_oid commit_oid;
	git_rebase_operation *operation = NULL;
	git_signature *signature = NULL;

	/** Get the signature to be used for the `commiter` field. */
	if ((error = git_signature_default(&signature, repo)) != 0) {
		handle_signature_create_error(error);

		goto cleanup;
	}

	/**
	 * If GIT_REBASE_NO_OPERATION, we've initialized, but haven't started
	 * the rebase. We'll start it via git_rebase_next.
	 */
	operation_index = git_rebase_operation_current(rebase);
	if (operation_index == GIT_REBASE_NO_OPERATION) {
		error = git_rebase_next(&operation, rebase);
		operation_index = git_rebase_operation_current(rebase);

		if (error || operation_index == GIT_REBASE_NO_OPERATION) {
			fprintf(stderr, "Error starting rebase!\n");
			error = -1;
			goto cleanup;
		}
	} else {
		operation = git_rebase_operation_byindex(rebase, operation_index);

		// When running `lg2 rebase --continue`, additional actions may need to be
		// taken.
		switch (operation->type) {
			case GIT_REBASE_OPERATION_EDIT:
				error = git_rebase_commit(&commit_oid,
					rebase,
					NULL, // Keep the original author
					signature, // Update the committer
					NULL, // Keep the message encoding
					NULL); // Keep the original message
				if (error == GIT_EUNMERGED) {
					fprintf(stderr,
						"Cannot continue rebase: There are still conflicts!\n"
						"Fix them (find them with `lg2 status`), then add the results "
						"with `lg2 add path/to/file/with/changes`.\n\n");
					goto cleanup;
				} else if (error == GIT_EAPPLIED) {
					fprintf(stderr,
						"The changes here have already been commited.\n"
						"Continuing without re-committing...\n\n");
					error = 0;
				} else if (error != 0) {
					fprintf(stderr, "Error while attempting to commit changes!\n");
					goto cleanup;
				} else {
					printf("Applied commit.\n");
				}

				error = git_rebase_next(&operation, rebase);
				break;
			default:
				break;
		}
	}

	while (error == 0 && operation != NULL) {
		git_commit *old_commit = NULL;
		const char *old_commit_message = "Error accessing old message.";
		char *new_commit_message = NULL;

		// operation->id is not set when a GIT_REBASE_OPERATION_EXEC.
		if (operation->type != GIT_REBASE_OPERATION_EXEC) {
			error = git_commit_lookup(&old_commit, repo, &operation->id);
			if (error) {
				fprintf(stderr, "Warning: Unable to look up commit message of current comit.\n");
				error = 0;
			} else {
				old_commit_message = git_commit_message(old_commit);
				printf("Applying commit `%s`...\n", old_commit_message);
			}
		}

		switch (operation->type) {
			case GIT_REBASE_OPERATION_EXEC:
				fprintf(stderr,
					"\nA part of the rebase is running the following command:\n"
					"\t%s\n"
					"The rebase has been paused to allow you to do so.\n"
					"Run `lg2 rebase --continue` to continue the rebase after"
					" running the command.\n",
					operation->exec);

				/** When we come back, start on the next operation. */
				git_rebase_next(&operation, rebase);
				error = -1;
				goto cleanup;
			case GIT_REBASE_OPERATION_EDIT:
				fprintf(stderr,
					"\nRebase paused. "
					"Run `lg2 rebase --continue` to continue the rebase.\n");
				goto cleanup;
			case GIT_REBASE_OPERATION_REWORD:
				printf("Current commit message: %s\n", old_commit_message);
				error = ask(&new_commit_message, "Change message to:", 1);
				if (error) goto cleanup;

				break;
			// TODO: Double-check that git_rebase_commit handles
			//       fixups, squashing, etc. for us.
			case GIT_REBASE_OPERATION_FIXUP:
			case GIT_REBASE_OPERATION_SQUASH:
			case GIT_REBASE_OPERATION_PICK:
				// Use the previous commit message.
				new_commit_message = NULL;
				break;
		}

		error = git_rebase_commit(&commit_oid, rebase,
				NULL,      // Keep the commit's author.
				signature, // Update the committer.
				NULL,      // Use UTF-8 for the message encoding.
				new_commit_message);
		if (error == GIT_EUNMERGED) {
			fprintf(stderr,
					"\nThere are merge conflicts! Please:\n"
					" * Fix each conflict (find them via `lg2 status`)\n"
					" * Add each fix to the index (via `lg2 add path/to/changed/file`)\n"
					" * Continue the rebase (with `lg2 rebase --continue`)\n"
					"Alternatively, you can cancel the rebase by running `lg2 rebase --abort`."
					"\n\n");
			goto cleanup;
		} else if (error == GIT_EAPPLIED) {
			fprintf(stderr, "  The commit has already been applied! Continuing.\n");
			error = 0;
		} else if (error != 0) {
			fprintf(stderr, "Error while committing!\n");
			goto cleanup;
		}

		error = git_rebase_next(&operation, rebase);
		operation_index = git_rebase_operation_current(rebase);

		free(new_commit_message);
		git_commit_free(old_commit);
	}

	error = git_rebase_finish(rebase, signature);
	if (error) {
		fprintf(stderr, "Unable to finish rebase.\n");
	} else {
		fprintf(stderr, "Finished rebasing!\n");
	}

cleanup:
	git_signature_free(signature);

	return error;
}

static int abort_rebase(git_rebase *rebase)
{
	int error = git_rebase_abort(rebase);

	if (error == GIT_ENOTFOUND) {
		fprintf(stderr, "It seems that there is no rebase to cancel.\n");
	}

	return error;
}

static int parse_args(rebase_options *options, git_repository *repo, int argc, char **argv)
{
	int i;
	memset(options, 0, sizeof(rebase_options));
	options->onto = NULL;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			break;
		} else if (strcmp(argv[i], "--continue") == 0) {
			printf("Continuing an existing rebase...\n");
			options->continue_existing = 1;
			return 0;
		} else if (strcmp(argv[i], "--abort") == 0) {
			printf("Stopping an existing rebase...\n");
			options->stop_existing = 1;
			return 0;
		} else if (*argv[i] == '-') {
			// Unrecognised argument.
			return -1;
		} else {
			break;
		}
	}

	// Now parsing 'upstream'.
	if (i == argc) {
		fprintf(stderr, "Not enough arguments.\n");
		return 1;
	}

	if (resolve_refish(&options->upstream, repo, argv[i]) != 0) {
		fprintf(stderr, "Unable to resolve upstream reference.\n");
		return -1;
	}

	return 0;
}

static void free_rebase_options(rebase_options *options)
{
	git_annotated_commit_free(options->upstream);
	git_annotated_commit_free(options->onto);
}

