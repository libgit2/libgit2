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

#define ERROR_MSG "Cannot perform reset"

static int reset_error_invalid(const char *msg)
{
	giterr_set(GITERR_INVALID, "%s - %s", ERROR_MSG, msg);
	return -1;
}

int git_reset(
	git_repository *repo,
	const git_object *target,
	git_reset_type reset_type)
{
	git_otype target_type = GIT_OBJ_BAD;
	git_object *commit = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	int error = -1;

	assert(repo && target);
	assert(reset_type == GIT_RESET_SOFT || reset_type == GIT_RESET_MIXED);

	if (git_object_owner(target) != repo)
		return reset_error_invalid("The given target does not belong to this repository.");

	if (reset_type == GIT_RESET_MIXED && git_repository_is_bare(repo))
		return reset_error_invalid("Mixed reset is not allowed in a bare repository.");

	target_type = git_object_type(target);

	switch (target_type)
	{
	case GIT_OBJ_TAG:
		if (git_tag_peel(&commit, (git_tag *)target) < 0)
			goto cleanup;

		if (git_object_type(commit) != GIT_OBJ_COMMIT) {
			reset_error_invalid("The given target does not resolve to a commit.");
			goto cleanup;
		}
		break;

	case GIT_OBJ_COMMIT:
		commit = (git_object *)target;
		break;

	default:
		return reset_error_invalid("Only git_tag and git_commit objects are valid targets.");
	}

	//TODO: Check for unmerged entries

	if (git_reference__update(repo, git_object_id(commit), GIT_HEAD_FILE) < 0)
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

	if (git_index_read_tree(index, tree) < 0) {
		giterr_set(GITERR_INDEX, "%s - Failed to update the index.", ERROR_MSG);
		goto cleanup;
	}

	if (git_index_write(index) < 0) {
		giterr_set(GITERR_INDEX, "%s - Failed to write the index.", ERROR_MSG);
		goto cleanup;
	}

	error = 0;

cleanup:
	if (target_type == GIT_OBJ_TAG)
		git_object_free(commit);

	git_index_free(index);
	git_tree_free(tree);

	return error;
}
