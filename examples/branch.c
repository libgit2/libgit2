/*
 * libgit2 "branch" example - shows how to manage branches
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
 * This example demonstrates the libgit2 branching APIs to roughly
 * simulate `git branch`.
 *
 * This does not have:
 *
 * - Robust error handling
 * - Most of the `git branch` options
 *
 * This does have:
 *
 * - Example of creating a branch
 * - Example of deleting a branch
 *
 */

#define NO_FORCE 0

int lg2_branch_create_from_head(git_repository *repo, char* branch_name)
{
	git_commit* current_head = NULL;
	git_reference* ref = NULL;
	int error = 0;

	error = get_repo_head(&current_head, repo);
	if (error != 0) {
		const git_error *err = git_error_last();
		char* message = err ? err->message : "No detailed messge.";
		fprintf(stderr, "Unable to look up HEAD: %s\n", message);

		goto cleanup;
	}

	error = git_branch_create(&ref, repo, branch_name, current_head, NO_FORCE);
	if (error != 0) {
		fprintf(stderr, "failed to create %s: %s\n", branch_name,
				git_error_last()->message);
		goto cleanup;
	}

cleanup:
	free(ref);
	free(current_head);
	return error;
}

int lg2_branch_delete(git_repository *repo, char* branch_name)
{
	git_reference* ref = NULL;
	int error = 0;

	error = git_branch_lookup(&ref, repo, branch_name, GIT_BRANCH_LOCAL);
	if (error == GIT_ENOTFOUND) {
		fprintf(stderr, "No local branch %s found, looking up remote.\n", branch_name);

		// No matching local branch: Lookup a remote branch
		error = git_branch_lookup(&ref, repo, branch_name, GIT_BRANCH_REMOTE);
	}
	if (error != 0) {
		fprintf(stderr, "Error looking up branch: %s\n", branch_name);
		goto cleanup;
	}

	error = git_branch_delete(ref);
	if (error != 0) {
		fprintf(stderr, "Error deleting branch.\n");
	}

cleanup:
	free(ref);
	return error;
}

int lg2_branch_set_upstream(git_repository *repo, char* branch_name)
{
	git_reference* local_branch = NULL;
	int error = 0;

	error = git_repository_head(&local_branch, repo);
	if (error != 0) {
		goto cleanup;
	}

	if (!git_reference_is_branch(local_branch)) {
		fprintf(stderr, "Error: Not currently on a branch.\n");
		error = -1;
		goto cleanup;
	}

	/**
	 * Here, branch_name can be NULL. If it is,
	 * we're clearing upstream information.
	 */
	error = git_branch_set_upstream(local_branch, branch_name);

cleanup:
	git_reference_free(local_branch);
	return error;
}

int lg2_branch_clear_upstream(git_repository *repo)
{
	return lg2_branch_set_upstream(repo, NULL);
}

int lg2_list_branches(git_repository *repo)
{
	git_branch_iterator *it = NULL;
	git_reference* current = NULL;
	int error = 0;

	error = git_branch_iterator_new(&it, repo, GIT_BRANCH_ALL);

	while (error == 0) {
		git_branch_t branch_type;
		git_reference* upstream = NULL;
		const char* name = NULL;

		error = git_branch_next(&current, &branch_type, it);
		if (error != 0) {
			if (error == GIT_ITEROVER) {
				error = 0;
				break;
			}

			fprintf(stderr, "Error while iterating over branches.\n");
			goto cleanup;
		}

		error = git_branch_name(&name, current);
		if (error != 0) {
			fprintf(stderr, "Error looking up branch name.\n");
			goto cleanup;
		}

		printf("%s", name);

		if (git_branch_is_checked_out(current)) {
			printf(" (Checked out, ");
		} else {
			printf(" (");
		}

		if (git_branch_is_head(current)) {
			printf("HEAD, ");
		}

		if (branch_type == GIT_BRANCH_REMOTE) {
			printf("remote");
		} else if (branch_type == GIT_BRANCH_LOCAL) {
			printf("local");
		} else {
			printf("ALL");
		}

		printf(")");

		if (branch_type == GIT_BRANCH_LOCAL) {
			/**
			 *  We can only query branch information for
			 * local branches.
			 */
			error = git_branch_upstream(&upstream, current);
			if (error == GIT_ENOTFOUND) {
				error = 0;
			} else if (error == 0) { // Success ->
				const char *upstream_name = git_reference_name(upstream);

				printf(" --> %s", upstream_name);
				git_reference_free(upstream);
			} else {
				const git_error* err = git_error_last();
				char* msg = err ? err->message : "Unspecified error.";
				printf(" --> %s\n", msg);
			}
		}

		printf("\n");
	}

cleanup:
	git_branch_iterator_free(it);
	return error;
}

int lg2_branch(git_repository *repo, int argc, char **argv)
{
	char* branch_name;
	int has_opt = (argc == 3);
	int delete_branch = (has_opt && strcmp(argv[1], "-d") == 0);
	int set_upstream = (has_opt && strcmp(argv[1], "-u") == 0);
	int list_branches = (argc == 2 && strcmp(argv[1], "--list") == 0);
	int unset_upstream = (argc == 2 && strcmp(argv[1], "--unset-upstream") == 0);

	if (argc < 2 || argc > 3 || (argc == 3 && !delete_branch && !set_upstream)
				|| strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		fprintf(stderr, "USAGE: branch <branch name>\n");
		fprintf(stderr, "           creates <branch name>\n");
		fprintf(stderr, "       branch -d <branch name>\n");
		fprintf(stderr, "           deletes <branch name>\n");
		fprintf(stderr, "       branch -u <upstream branch name>\n");
		fprintf(stderr, "           sets the current branch's upstream (upstream must exist).\n");
		fprintf(stderr, "       branch --unset-upstream\n");
		fprintf(stderr, "           clears upstream information for the current branch.\n");
		fprintf(stderr, "       branch --list\n");
		fprintf(stderr, "           lists local and remote branches.\n");
		fprintf(stderr, "WARNING: The form of this command's output is unstable.\n");
		return -1;
	}

	branch_name = argv[argc - 1];
	if (delete_branch) {
		return lg2_branch_delete(repo, branch_name);
	} else if (set_upstream) {
		return lg2_branch_set_upstream(repo, branch_name);
	} else if (unset_upstream) {
		return lg2_branch_clear_upstream(repo);
	} else if (list_branches) {
		return lg2_list_branches(repo);
	} else {
		return lg2_branch_create_from_head(repo, branch_name);
	}
}

