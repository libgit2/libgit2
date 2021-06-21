/*
 * libgit2 "push" example - shows how to push to remote
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
 * simulate `git push`.
 *
 * This does not have:
 *
 * - Robust error handling
 * - Most of the `git push` options
 *
 * This does have:
 *
 * - Example of push to origin/master
 *
 */

typedef struct {
	char *remote_name;     // Not owned by.
	git_remote *remote;    // Owned by.

	git_strarray refspecs; // Owned by; lifetime must be \leq argv lifetime.
	int start_tracking;
	int force_push;
} push_opts;
static void push_opts_free(push_opts *);

/** Parse commandline options, returns non-zero on failure. */
static int parse_args(git_repository *repo, push_opts *opts, int argc, char **argv);

static int start_tracking(git_repository *, git_refspec *);
static int push_status_cb(const char *refname, const char *status, void *payload);

/** Entry point for this command */
int lg2_push(git_repository *repo, int argc, char **argv)
{
	push_opts cmdline_opts;
	int error = 0;

	git_push_options options;
	git_remote_callbacks callbacks;

    /* Validate args */
	if (parse_args(repo, &cmdline_opts, argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s\n", argv[0]);
		fprintf(stderr, "          Push, using 'origin' as the remote.\n");
		fprintf(stderr, "       %s %s\n", argv[0], "<remote> <refspec>*");
		fprintf(stderr, "          Push to the given remote. If <refspec> is not\n"
						"          present, the current branch must be tracking an\n"
						"          upstream branch in <remote>.\n");
		fprintf(stderr, "       %s --force %s\n"
						"          Force updating each of the given refspecs.\n",
						argv[0], "<remote> <refspec>*");
		fprintf(stderr, "       %s -u %s %s\n", argv[0], "<remote>", "<refspec>*");
		fprintf(stderr, "          Act like %s %s %s, but start tracking\n"
						"          each successfully pushed branch's upstream.\n",
							argv[0], "<remote>", "<refspec>*");

		error = -1;
		goto cleanup;
	}

	check_lg2(git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION),
			"Error initializing remote callbacks", NULL);

	callbacks.certificate_check = certificate_confirm_cb;
	callbacks.credentials = cred_acquire_cb;
	callbacks.payload = repo;

	callbacks.push_update_reference = push_status_cb;

	check_lg2(git_push_options_init(&options, GIT_PUSH_OPTIONS_VERSION ), "Error initializing push", NULL);
	options.callbacks = callbacks;

	check_lg2(git_remote_push(cmdline_opts.remote, &cmdline_opts.refspecs, &options),
			"Error pushing", NULL);

	printf("pushed\n");

	if (cmdline_opts.start_tracking) {
		size_t i;
		for (i = 0; i < cmdline_opts.refspecs.count; i++) {
			git_refspec *refspec;
			check_lg2(git_refspec_parse(&refspec, cmdline_opts.refspecs.strings[i], 0),
					"Refspec parse error!", NULL);

			error = start_tracking(repo, refspec);
			if (error != 0) {
				const char *src = git_refspec_src(refspec);
				const char *dst = git_refspec_dst(refspec);

				fprintf(stderr, "Can't make %s track %s.\n", src, dst);
				fprintf(stderr,
					" This may be because one of the given refspecs is formatted incorrectly.\n"
					"These are some example refspecs:\n"
					"   +refs/heads/localbranch1:refs/remotes/origin/make_it_track_this\n"
					"        Here, the '+' means we're forcing the update.\n"
					"   refs/remotes/origin/somebranch\n"
					"        Updates somebranch with the contents of this branch, "
					" but doesn't force it.\n"
					" ");

				fprintf(stderr,
					" You can also try `lg2 branch -u '%s'`, "
					"which sets the current branch's upstream to %s.\n",
					dst, dst);
			}

			git_refspec_free(refspec);
		}
	}

cleanup:
	push_opts_free(&cmdline_opts);
	return error;
}

static int push_status_cb(const char *refname, const char *status, void *payload)
{
	UNUSED(payload);

	if (status != NULL) {
		fprintf(stderr, "ERROR updating %s: %s\n", refname, status);
		fprintf(stderr,
			"If you want to create an upstream branch or overwrite\n"
			"upstream changes, you may want to try running\n"
			"    lg2 push --force <remote name> <refspec>...\n"
			"or just\n"
			"	lg2 push --force\t\t to update only the current branch.\n"
			"This forces the remote to accept local changes, but\n"
			"may overwrite other changes!\n");

		// An error!
		return -1;
	}

	return 0;
}

static int start_tracking(git_repository *repo, git_refspec *refspec)
{
	git_reference *src = NULL;
	git_reference *dst = NULL;
	git_reference *current = NULL;
	git_branch_t current_type;
	git_branch_iterator *it = NULL;
	int error = 0;

	error = git_branch_iterator_new(&it, repo, GIT_BRANCH_ALL);
	if (error != 0) {
		fprintf(stderr, "Error creating branch iterator.\n");
		goto cleanup;
	}

	while ((error = git_branch_next(&current, &current_type, it)) == 0) {
		const char* refname = git_reference_name(current);

		if (current_type == GIT_BRANCH_LOCAL && git_refspec_src_matches(refspec, refname)) {
			if (src != NULL) {
				fprintf(stderr, "Error: Multiple local branches match %s. "
								" Desired branch is ambiguous.\n",
								git_refspec_src(refspec));
				error = -1;
				goto cleanup;
			}

			src = current;
		} else if (current_type == GIT_BRANCH_REMOTE && git_refspec_dst_matches(refspec, refname)) {
			if (dst != NULL) {
				fprintf(stderr, "Error: Multiple remote branches match %s. "
								" Desired branch is ambiguous.\n",
								git_refspec_dst(refspec));
				error = -1;
				goto cleanup;
			}

			dst = current;
		}

	}

	if (error != GIT_ITEROVER) {
		fprintf(stderr, "Error iterating over branches.\n");
		goto cleanup;
	}

	if (src != NULL && dst != NULL) {
		// We now have src that we want to track dst:
		const char* dst_name = git_reference_name(dst);
		const char* src_name = git_reference_name(src);
		if (src_name != NULL && dst_name != NULL) {
			error = git_branch_set_upstream(src, dst_name);
		} else {
			error = -1;
		}

		if (error != 0) {
			fprintf(stderr, "Error: Unable to set %s's upstream to %s.\n",
					src_name,
					dst_name);
			goto cleanup;
		}

		printf("Successfully set %s to track %s.\n", src_name, dst_name);
	} else {
		printf("Error: No branch for ");
		if (src == NULL) {
			printf("source %s", dst == NULL ? "and " : "");
		}

		if (dst == NULL) {
			printf("destination ");
		}

		printf("of %s\n", git_refspec_string(refspec));
		error = -1;
		goto cleanup;
	}
cleanup:
	git_branch_iterator_free(it);
	git_reference_free(src);
	git_reference_free(dst);
	return error;
}

/// Returns a heap-allocated copy of [arg] (formatted).
static char * cpy_and_format_refspec_arg(const char *arg, push_opts *opts)
{
	char *result;

	if (opts->force_push && arg[0] != '+') {
		// If we're force-pushing, prefix the path with a '+'.
		result = (char*) malloc(strlen(arg) + 2);
		result[0] = '+';

		strcpy(result + 1, arg);
	} else {
		result = (char*) malloc(strlen(arg) + 1);
		strcpy(result, arg);
	}

	return result;
}

static int parse_args(git_repository *repo, push_opts *opts, int argc, char **argv)
{
	int i;
	memset(opts, 0, sizeof(push_opts));

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (strncmp(arg, "-", 1) != 0) {
			break;
		} else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			// Non-zero exit status: Show help.
			return 1;
		} else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--set-upstream") == 0) {
			opts->start_tracking = 1;
		} else if (strcmp(arg, "--force") == 0) {
			opts->force_push = 1;
		} else {
			fprintf(stderr, "Unknown argument: %s\n", arg);
			return 1;
		}
	}

	if (i == argc) {
		opts->remote_name = "origin";
	} else {
		// push <options>... <remote> <refspec>... case
		//                      ^^^
		//                       i
		opts->remote_name = argv[i];
	}

	// Get the remote.
	check_lg2(git_remote_lookup(&opts->remote, repo, opts->remote_name), "Unable to lookup remote", NULL);

	i++;
	if (i < argc) {
		size_t j;

		// push <options>... <remote> <refspec>... case
		//                               ^^^
		//                                i

		opts->refspecs.count = argc - i;
		opts->refspecs.strings = (char**) malloc(opts->refspecs.count * sizeof(char*));

		for (j = 0; j < opts->refspecs.count; j++) {
			char *arg = argv[j + i];
			opts->refspecs.strings[j] = cpy_and_format_refspec_arg(arg, opts);
		}
	} else {
		git_reference *head;
		const char *branch_name;
		int err = 0;

		// No given refspecs, so just push the current branch.
		err = git_repository_head(&head, repo);
		if (err == GIT_EUNBORNBRANCH || err == GIT_ENOTFOUND) {
			fprintf(stderr, "Unable to find HEAD!\n");
			return 1;
		}

		branch_name = git_reference_name(head);
		if (branch_name == NULL) {
			fprintf(stderr, "Cannot push current branch; not currently on a branch.\n");
			git_reference_free(head);
			return 1;
		}

		opts->refspecs.count = 1;
		opts->refspecs.strings = (char**) malloc(1 * sizeof(char*));
		opts->refspecs.strings[0] = cpy_and_format_refspec_arg(branch_name, opts);

		git_reference_free(head);

		printf("No refspecs given. Pushing: %s\n", opts->refspecs.strings[0]);
	}

	return 0;
}

static void push_opts_free(push_opts *opts)
{
	// Frees the string array in refspecs.
	git_strarray_dispose(&opts->refspecs);

	git_remote_free(opts->remote);
	opts->remote = NULL;
}

