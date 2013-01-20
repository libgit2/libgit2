/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "tag.h"
#include "merge.h"
#include "diff.h"
#include "git2/reset.h"
#include "git2/checkout.h"
#include "git2/merge.h"

#define ERROR_MSG "Cannot perform reset"

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

		if ((error = git_reference_create(
			&target,
			repo,
			git_reference_symbolic_target(head),
			git_object_id(commit), 0)) < 0)
				goto cleanup;
	} else {
		if ((error = git_reference_set_target(head, git_object_id(commit))) < 0)
			goto cleanup;
	}

	error = 0;

cleanup:
	git_reference_free(head);
	git_reference_free(target);
	return error;
}

int git_reset_default(
	git_repository *repo,
	git_object *target,
	git_strarray* pathspecs)
{
	git_object *commit = NULL;
	git_tree *tree = NULL;
	git_diff_list *diff = NULL;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	size_t i;
	git_diff_delta *delta;
	git_index_entry entry;
	int error;
	git_index *index = NULL;

	assert(pathspecs != NULL && pathspecs->count > 0); 

	memset(&entry, 0, sizeof(git_index_entry));

	if ((error = git_repository_index(&index, repo)) < 0)
		goto cleanup;

	if (target) {
		if (git_object_owner(target) != repo) {
			giterr_set(GITERR_OBJECT,
				"%s_default - The given target does not belong to this repository.", ERROR_MSG);
			return -1;
		}

		if ((error = git_object_peel(&commit, target, GIT_OBJ_COMMIT)) < 0 ||
			(error = git_commit_tree(&tree, (git_commit *)commit)) < 0)
			goto cleanup;
	}

	opts.pathspec = *pathspecs;
	opts.flags = GIT_DIFF_REVERSE;

	if ((error = git_diff_tree_to_index(
		&diff, repo, tree, index, &opts)) < 0)
			goto cleanup;

	git_vector_foreach(&diff->deltas, i, delta) {
		if ((error = git_index_conflict_remove(index, delta->old_file.path)) < 0)
			goto cleanup;

		assert(delta->status == GIT_DELTA_ADDED ||
			delta->status == GIT_DELTA_MODIFIED ||
			delta->status == GIT_DELTA_DELETED);

		if (delta->status == GIT_DELTA_DELETED) {
			if ((error = git_index_remove(index, delta->old_file.path, 0)) < 0)
				goto cleanup;
		} else {
			entry.mode = delta->new_file.mode;
			git_oid_cpy(&entry.oid, &delta->new_file.oid);
			entry.path = (char *)delta->new_file.path;

			if ((error = git_index_add(index, &entry)) < 0)
				goto cleanup;
		}
	}

	error = git_index_write(index);

cleanup:
	git_object_free(commit);
	git_tree_free(tree);
	git_index_free(index);
	git_diff_list_free(diff);

	return error;
}

int git_reset(
	git_repository *repo,
	git_object *target,
	git_reset_t reset_type)
{
	git_object *commit = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	int error = 0;
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	assert(repo && target);

	if (git_object_owner(target) != repo) {
		giterr_set(GITERR_OBJECT,
			"%s - The given target does not belong to this repository.", ERROR_MSG);
		return -1;
	}

	if (reset_type != GIT_RESET_SOFT &&
		(error = git_repository__ensure_not_bare(repo,
			reset_type == GIT_RESET_MIXED ? "reset mixed" : "reset hard")) < 0)
		return error;

	if ((error = git_object_peel(&commit, target, GIT_OBJ_COMMIT)) < 0 ||
		(error = git_repository_index(&index, repo)) < 0 ||
		(error = git_commit_tree(&tree, (git_commit *)commit)) < 0)
		goto cleanup;

	if (reset_type == GIT_RESET_SOFT &&
		(git_repository_state(repo) == GIT_REPOSITORY_STATE_MERGE ||
		 git_index_has_conflicts(index)))
	{
		giterr_set(GITERR_OBJECT, "%s (soft) in the middle of a merge.", ERROR_MSG);
		error = GIT_EUNMERGED;
		goto cleanup;
	}

	/* move HEAD to the new target */
	if ((error = update_head(repo, commit)) < 0)
		goto cleanup;

	if (reset_type == GIT_RESET_HARD) {
		/* overwrite working directory with HEAD */
		opts.checkout_strategy = GIT_CHECKOUT_FORCE;

		if ((error = git_checkout_tree(repo, (git_object *)tree, &opts)) < 0)
			goto cleanup;
	}

	if (reset_type > GIT_RESET_SOFT) {
		/* reset index to the target content */

		if ((error = git_index_read_tree(index, tree)) < 0 ||
			(error = git_index_write(index)) < 0)
			goto cleanup;

		if ((error = git_repository_merge_cleanup(repo)) < 0) {
			giterr_set(GITERR_INDEX, "%s - failed to clean up merge data", ERROR_MSG);
			goto cleanup;
		}
	}

cleanup:
	git_object_free(commit);
	git_index_free(index);
	git_tree_free(tree);

	return error;
}
