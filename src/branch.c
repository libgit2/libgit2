/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "branch.h"
#include "tag.h"

static int retrieve_branch_reference(
	git_reference **branch_reference_out,
	git_repository *repo,
	const char *branch_name,
	int is_remote)
{
	git_reference *branch;
	int error = -1;
	char *prefix;
	git_buf ref_name = GIT_BUF_INIT;

	*branch_reference_out = NULL;

	prefix = is_remote ? GIT_REFS_REMOTES_DIR : GIT_REFS_HEADS_DIR;

	if (git_buf_joinpath(&ref_name, prefix, branch_name) < 0)
		goto cleanup;

	if ((error = git_reference_lookup(&branch, repo, ref_name.ptr)) < 0) {
		giterr_set(GITERR_REFERENCE,
			"Cannot locate %s branch '%s'.", is_remote ? "remote-tracking" : "local", branch_name);
		goto cleanup;
	}

	*branch_reference_out = branch;

cleanup:
	git_buf_free(&ref_name);
	return error;
}

static int create_error_invalid(const char *msg)
{
	giterr_set(GITERR_INVALID, "Cannot create branch - %s", msg);
	return -1;
}

int git_branch_create(
		git_oid *oid_out,
		git_repository *repo,
		const char *branch_name,
		const git_object *target,
		int force)
{
	git_otype target_type = GIT_OBJ_BAD;
	git_object *commit = NULL;
	git_reference *branch = NULL;
	git_buf canonical_branch_name = GIT_BUF_INIT;
	int error = -1;

	assert(repo && branch_name && target && oid_out);

	if (git_object_owner(target) != repo)
		return create_error_invalid("The given target does not belong to this repository");

	target_type = git_object_type(target);

	switch (target_type)
	{
	case GIT_OBJ_TAG:
		if (git_tag_peel(&commit, (git_tag *)target) < 0)
			goto cleanup;

		if (git_object_type(commit) != GIT_OBJ_COMMIT) {
			create_error_invalid("The given target does not resolve to a commit");
			goto cleanup;
		}
		break;

	case GIT_OBJ_COMMIT:
		commit = (git_object *)target;
		break;

	default:
		return create_error_invalid("Only git_tag and git_commit objects are valid targets.");
	}

	if (git_buf_joinpath(&canonical_branch_name, GIT_REFS_HEADS_DIR, branch_name) < 0)
		goto cleanup;

	if (git_reference_create_oid(&branch, repo, git_buf_cstr(&canonical_branch_name), git_object_id(commit), force) < 0)
		goto cleanup;

	git_oid_cpy(oid_out, git_reference_oid(branch));
	error = 0;

cleanup:
	if (target_type == GIT_OBJ_TAG)
		git_object_free(commit);

	git_reference_free(branch);
	git_buf_free(&canonical_branch_name);
	return error;
}

int git_branch_delete(git_repository *repo, const char *branch_name, git_branch_t branch_type)
{
	git_reference *branch = NULL;
	git_reference *head = NULL;
	int error;

	assert((branch_type == GIT_BRANCH_LOCAL) || (branch_type == GIT_BRANCH_REMOTE));

	if ((error = retrieve_branch_reference(&branch, repo, branch_name, branch_type == GIT_BRANCH_REMOTE)) < 0)
		return error;

	if (git_reference_lookup(&head, repo, GIT_HEAD_FILE) < 0) {
		giterr_set(GITERR_REFERENCE, "Cannot locate HEAD.");
		goto on_error;
	}

	if ((git_reference_type(head) == GIT_REF_SYMBOLIC)
		&& (strcmp(git_reference_target(head), git_reference_name(branch)) == 0)) {
			giterr_set(GITERR_REFERENCE,
					"Cannot delete branch '%s' as it is the current HEAD of the repository.", branch_name);
			goto on_error;
	}

	if (git_reference_delete(branch) < 0)
		goto on_error;

	git_reference_free(head);
	return 0;

on_error:
	git_reference_free(head);
	git_reference_free(branch);
	return -1;
}

typedef struct {
	git_vector *branchlist;
	unsigned int branch_type;
} branch_filter_data;

static int branch_list_cb(const char *branch_name, void *payload)
{
	branch_filter_data *filter = (branch_filter_data *)payload;

	if ((filter->branch_type & GIT_BRANCH_LOCAL && git__prefixcmp(branch_name, GIT_REFS_HEADS_DIR) == 0)
		|| (filter->branch_type & GIT_BRANCH_REMOTE && git__prefixcmp(branch_name, GIT_REFS_REMOTES_DIR) == 0))
		return git_vector_insert(filter->branchlist, git__strdup(branch_name));

	return 0;
}

int git_branch_list(git_strarray *branch_names, git_repository *repo, unsigned int list_flags)
{
	int error;
	branch_filter_data filter;
	git_vector branchlist;

	assert(branch_names && repo);

	if (git_vector_init(&branchlist, 8, NULL) < 0)
		return -1;

	filter.branchlist = &branchlist;
	filter.branch_type = list_flags;

	error = git_reference_foreach(repo, GIT_REF_LISTALL, &branch_list_cb, (void *)&filter);
	if (error < 0) {
		git_vector_free(&branchlist);
		return -1;
	}

	branch_names->strings = (char **)branchlist.contents;
	branch_names->count = branchlist.length;
	return 0;
}

int git_branch_move(git_repository *repo, const char *old_branch_name, const char *new_branch_name, int force)
{
	git_reference *reference = NULL;
	git_buf old_reference_name = GIT_BUF_INIT, new_reference_name = GIT_BUF_INIT;
	int error = 0;

	if ((error = git_buf_joinpath(&old_reference_name, GIT_REFS_HEADS_DIR, old_branch_name)) < 0)
		goto cleanup;

	/* We need to be able to return GIT_ENOTFOUND */
	if ((error = git_reference_lookup(&reference, repo, git_buf_cstr(&old_reference_name))) < 0)
		goto cleanup;

	if ((error = git_buf_joinpath(&new_reference_name, GIT_REFS_HEADS_DIR, new_branch_name)) < 0)
		goto cleanup;

	error = git_reference_rename(reference, git_buf_cstr(&new_reference_name), force);

cleanup:
	git_reference_free(reference);
	git_buf_free(&old_reference_name);
	git_buf_free(&new_reference_name);

	return error;
}
