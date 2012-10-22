/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "tag.h"
#include "git2/reset.h"
#include "git2/checkout.h"

#define ERROR_MSG "Cannot perform reset"

static int reset_error_invalid(const char *msg)
{
	giterr_set(GITERR_INVALID, "%s - %s", ERROR_MSG, msg);
	return -1;
}

static int update_head(git_repository *repo, git_object *commit)
{
	int error;
	git_reference *head = NULL, *target = NULL;

	error = git_repository_head(&head, repo);

	if (error < 0 && error != GIT_EORPHANEDHEAD)
		return error;

	if (error == GIT_EORPHANEDHEAD) {
		giterr_clear();

		/*
		 * TODO: This is a bit weak as this doesn't support chained
		 * symbolic references. yet.
		 */
		if ((error = git_reference_lookup(&head, repo, GIT_HEAD_FILE)) < 0)
			goto cleanup;

		if ((error = git_reference_create_oid(
			&target,
			repo,
			git_reference_target(head),
			git_object_id(commit), 0)) < 0)
				goto cleanup;
	} else {
		if ((error = git_reference_set_oid(head, git_object_id(commit))) < 0)
			goto cleanup;
	}

	error = 0;

cleanup:
	git_reference_free(head);
	git_reference_free(target);
	return error;
}

int git_reset(
	git_repository *repo,
	git_object *target,
	git_reset_type reset_type)
{
	git_object *commit = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	int error = -1;
	git_checkout_opts opts;

	assert(repo && target);
	assert(reset_type == GIT_RESET_SOFT
		|| reset_type == GIT_RESET_MIXED
		|| reset_type == GIT_RESET_HARD);

	if (git_object_owner(target) != repo)
		return reset_error_invalid("The given target does not belong to this repository.");

	if (reset_type != GIT_RESET_SOFT
		&& git_repository__ensure_not_bare(
			repo,
			reset_type == GIT_RESET_MIXED ? "reset mixed" : "reset hard") < 0)
				return GIT_EBAREREPO;

	if (git_object_peel(&commit, target, GIT_OBJ_COMMIT) < 0) {
		reset_error_invalid("The given target does not resolve to a commit");
		goto cleanup;
	}

	//TODO: Check for unmerged entries

	if (update_head(repo, commit) < 0)
		goto cleanup;

	if (reset_type == GIT_RESET_SOFT) {
		error = 0;
		goto cleanup;
	}

	if (git_commit_tree(&tree, (git_commit *)commit) < 0) {
		giterr_set(GITERR_OBJECT, "%s - Failed to retrieve the commit tree.", ERROR_MSG);
		goto cleanup;
	}

	if (git_repository_index(&index, repo) < 0) {
		giterr_set(GITERR_OBJECT, "%s - Failed to retrieve the index.", ERROR_MSG);
		goto cleanup;
	}

	if (git_index_read_tree(index, tree, NULL) < 0) {
		giterr_set(GITERR_INDEX, "%s - Failed to update the index.", ERROR_MSG);
		goto cleanup;
	}

	if (git_index_write(index) < 0) {
		giterr_set(GITERR_INDEX, "%s - Failed to write the index.", ERROR_MSG);
		goto cleanup;
	}

	if (reset_type == GIT_RESET_MIXED) {
		error = 0;
		goto cleanup;
	}

	memset(&opts, 0, sizeof(opts));
	opts.checkout_strategy =
		GIT_CHECKOUT_CREATE_MISSING
		| GIT_CHECKOUT_OVERWRITE_MODIFIED
		| GIT_CHECKOUT_REMOVE_UNTRACKED;

	if (git_checkout_index(repo, &opts, NULL) < 0) {
		giterr_set(GITERR_INDEX, "%s - Failed to checkout the index.", ERROR_MSG);
		goto cleanup;
	}

	error = 0;

cleanup:
	git_object_free(commit);
	git_index_free(index);
	git_tree_free(tree);

	return error;
}
