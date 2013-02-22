/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "tag.h"
#include "config.h"
#include "refspec.h"
#include "refs.h"

#include "git2/branch.h"

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

static int not_a_local_branch(const char *reference_name)
{
	giterr_set(
		GITERR_INVALID,
		"Reference '%s' is not a local branch.", reference_name);
	return -1;
}

int git_branch_create(
		git_reference **ref_out,
		git_repository *repository,
		const char *branch_name,
		const git_commit *commit,
		int force)
{
	git_reference *branch = NULL;
	git_buf canonical_branch_name = GIT_BUF_INIT;
	int error = -1;

	assert(branch_name && commit && ref_out);
	assert(git_object_owner((const git_object *)commit) == repository);

	if (git_buf_joinpath(&canonical_branch_name, GIT_REFS_HEADS_DIR, branch_name) < 0)
		goto cleanup;

	error = git_reference_create(&branch, repository,
		git_buf_cstr(&canonical_branch_name), git_commit_id(commit), force);

	if (!error)
		*ref_out = branch;

cleanup:
	git_buf_free(&canonical_branch_name);
	return error;
}

int git_branch_delete(git_reference *branch)
{
	int is_head;
	git_buf config_section = GIT_BUF_INIT;
	int error = -1;

	assert(branch);

	if (!git_reference_is_branch(branch) &&
		!git_reference_is_remote(branch)) {
		giterr_set(GITERR_INVALID, "Reference '%s' is not a valid branch.", git_reference_name(branch));
		return -1;
	}

	if ((is_head = git_branch_is_head(branch)) < 0)
		return is_head;

	if (is_head) {
		giterr_set(GITERR_REFERENCE,
				"Cannot delete branch '%s' as it is the current HEAD of the repository.", git_reference_name(branch));
		return -1;
	}

	if (git_buf_printf(&config_section, "branch.%s", git_reference_name(branch) + strlen(GIT_REFS_HEADS_DIR)) < 0)
		goto on_error;

	if (git_config_rename_section(
		git_reference_owner(branch), 
		git_buf_cstr(&config_section),
		NULL) < 0)
			goto on_error;

	if (git_reference_delete(branch) < 0)
		goto on_error;

	error = 0;

on_error:
	git_buf_free(&config_section);
	return error;
}

typedef struct {
	int (*branch_cb)(
			const char *branch_name,
			git_branch_t branch_type,
			void *payload);
	void *callback_payload;
	unsigned int branch_type;
} branch_foreach_filter;

static int branch_foreach_cb(const char *branch_name, void *payload)
{
	branch_foreach_filter *filter = (branch_foreach_filter *)payload;

	if (filter->branch_type & GIT_BRANCH_LOCAL &&
		git__prefixcmp(branch_name, GIT_REFS_HEADS_DIR) == 0)
		return filter->branch_cb(branch_name + strlen(GIT_REFS_HEADS_DIR), GIT_BRANCH_LOCAL, filter->callback_payload);

	if (filter->branch_type & GIT_BRANCH_REMOTE &&
		git__prefixcmp(branch_name, GIT_REFS_REMOTES_DIR) == 0)
		return filter->branch_cb(branch_name + strlen(GIT_REFS_REMOTES_DIR), GIT_BRANCH_REMOTE, filter->callback_payload);

	return 0;
}

int git_branch_foreach(
		git_repository *repo,
		unsigned int list_flags,
		int (*branch_cb)(
			const char *branch_name,
			git_branch_t branch_type,
			void *payload),
		void *payload
)
{
	branch_foreach_filter filter;

	filter.branch_cb = branch_cb;
	filter.branch_type = list_flags;
	filter.callback_payload = payload;

	return git_reference_foreach(repo, GIT_REF_LISTALL, &branch_foreach_cb, (void *)&filter);
}

int git_branch_move(
	git_reference *branch,
	const char *new_branch_name,
	int force)
{
	git_buf new_reference_name = GIT_BUF_INIT,
		old_config_section = GIT_BUF_INIT,
		new_config_section = GIT_BUF_INIT;
	int error;

	assert(branch && new_branch_name);

	if (!git_reference_is_branch(branch))
		return not_a_local_branch(git_reference_name(branch));

	if ((error = git_buf_joinpath(&new_reference_name, GIT_REFS_HEADS_DIR, new_branch_name)) < 0)
		goto cleanup;

	if (git_buf_printf(
		&old_config_section,
		"branch.%s",
		git_reference_name(branch) + strlen(GIT_REFS_HEADS_DIR)) < 0)
			goto cleanup;

	if ((error = git_reference_rename(branch, git_buf_cstr(&new_reference_name), force)) < 0)
		goto cleanup;

	if (git_buf_printf(&new_config_section, "branch.%s", new_branch_name) < 0)
		goto cleanup;

	if ((error = git_config_rename_section(
		git_reference_owner(branch), 
		git_buf_cstr(&old_config_section),
		git_buf_cstr(&new_config_section))) < 0)
			goto cleanup;

cleanup:
	git_buf_free(&new_reference_name);
	git_buf_free(&old_config_section);
	git_buf_free(&new_config_section);

	return error;
}

int git_branch_lookup(
		git_reference **ref_out,
		git_repository *repo,
		const char *branch_name,
		git_branch_t branch_type)
{
	assert(ref_out && repo && branch_name);

	return retrieve_branch_reference(ref_out, repo, branch_name, branch_type == GIT_BRANCH_REMOTE);
}

int git_branch_name(const char **out, git_reference *ref)
{
	const char *branch_name;

	assert(out && ref);

	branch_name = ref->name;

	if (git_reference_is_branch(ref)) {
		branch_name += strlen(GIT_REFS_HEADS_DIR);
	} else if (git_reference_is_remote(ref)) {
		branch_name += strlen(GIT_REFS_REMOTES_DIR);
	} else {
		giterr_set(GITERR_INVALID,
				"Reference '%s' is neither a local nor a remote branch.", ref->name);
		return -1;
	}
	*out = branch_name;
	return 0;
}

static int retrieve_tracking_configuration(
	const char **out,
	git_repository *repo,
	const char *canonical_branch_name,
	const char *format)
{
	git_config *config;
	git_buf buf = GIT_BUF_INIT;
	int error;

	if (git_repository_config__weakptr(&config, repo) < 0)
		return -1;

	if (git_buf_printf(&buf, format,
		canonical_branch_name + strlen(GIT_REFS_HEADS_DIR)) < 0)
			return -1;

	error = git_config_get_string(out, config, git_buf_cstr(&buf));
	git_buf_free(&buf);
	return error;
}

int git_branch_tracking__name(
	git_buf *tracking_name,
	git_repository *repo,
	const char *canonical_branch_name)
{
	const char *remote_name, *merge_name;
	git_buf buf = GIT_BUF_INIT;
	int error = -1;
	git_remote *remote = NULL;
	const git_refspec *refspec;

	assert(tracking_name && canonical_branch_name);

	if (!git_reference__is_branch(canonical_branch_name))
		return not_a_local_branch(canonical_branch_name);

	if ((error = retrieve_tracking_configuration(
		&remote_name, repo, canonical_branch_name, "branch.%s.remote")) < 0)
			goto cleanup;

	if ((error = retrieve_tracking_configuration(
		&merge_name, repo, canonical_branch_name, "branch.%s.merge")) < 0)
			goto cleanup;

	if (!*remote_name || !*merge_name) {
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	if (strcmp(".", remote_name) != 0) {
		if ((error = git_remote_load(&remote, repo, remote_name)) < 0)
			goto cleanup;

		refspec = git_remote_fetchspec(remote);
		if (refspec == NULL
			|| refspec->src == NULL
			|| refspec->dst == NULL) {
				error = GIT_ENOTFOUND;
				goto cleanup;
		}

		if (git_refspec_transform_r(&buf, refspec, merge_name) < 0)
			goto cleanup;
	} else
		if (git_buf_sets(&buf, merge_name) < 0)
			goto cleanup;

	error = git_buf_set(tracking_name, git_buf_cstr(&buf), git_buf_len(&buf));

cleanup:
	git_remote_free(remote);
	git_buf_free(&buf);
	return error;
}

int git_branch_remote_name(
	char *remote_name_out,
	size_t buffer_size,
	git_repository *repo,
	const char *canonical_branch_name)
{
	git_strarray remote_list = {0};
	size_t i, remote_name_size;
	git_remote *remote;
	const git_refspec *fetchspec;
	int error = 0;
	char *remote_name = NULL;

	assert(repo && canonical_branch_name);

	if (remote_name_out && buffer_size)
		*remote_name_out = '\0';

	/* Verify that this is a remote branch */
	if (!git_reference__is_remote(canonical_branch_name)) {
		giterr_set(GITERR_INVALID, "Reference '%s' is not a remote branch.",
			canonical_branch_name);
		error = GIT_ERROR;
		goto cleanup;
	}

	/* Get the remotes */
	if ((error = git_remote_list(&remote_list, repo)) < 0)
		goto cleanup;

	/* Find matching remotes */
	for (i = 0; i < remote_list.count; i++) {
		if ((error = git_remote_load(&remote, repo, remote_list.strings[i])) < 0)
			goto cleanup;

		fetchspec = git_remote_fetchspec(remote);

		/* Defensivly check that we have a fetchspec */
		if (fetchspec &&
			git_refspec_dst_matches(fetchspec, canonical_branch_name)) {
			/* If we have not already set out yet, then set
			 * it to the matching remote name. Otherwise
			 * multiple remotes match this reference, and it
			 * is ambiguous. */
			if (!remote_name) {
				remote_name = remote_list.strings[i];
			} else {
				git_remote_free(remote);
				error = GIT_EAMBIGUOUS;
				goto cleanup;
			}
		}

		git_remote_free(remote);
	}

	if (remote_name) {
		remote_name_size = strlen(remote_name) + 1;
		error = (int) remote_name_size;

		if (remote_name_out) {
			if(remote_name_size > buffer_size) {
				giterr_set(
					GITERR_INVALID,
					"Buffer too short to hold the remote name.");
				error = GIT_ERROR;
				goto cleanup;
			}

			memcpy(remote_name_out, remote_name, remote_name_size);
		}
	} else {
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

cleanup:
	git_strarray_free(&remote_list);
	return error;
}

int git_branch_tracking_name(
	char *tracking_branch_name_out,
	size_t buffer_size,
	git_repository *repo,
	const char *canonical_branch_name)
{
	git_buf buf = GIT_BUF_INIT;
	int error;

	assert(canonical_branch_name);

	if (tracking_branch_name_out && buffer_size)
		*tracking_branch_name_out = '\0';

	if ((error = git_branch_tracking__name(
		&buf, repo, canonical_branch_name)) < 0)
			goto cleanup;

	if (tracking_branch_name_out && buf.size + 1 > buffer_size) { /* +1 for NUL byte */
		giterr_set(
			GITERR_INVALID,
			"Buffer too short to hold the tracked reference name.");
		error = -1;
		goto cleanup;
	}

	if (tracking_branch_name_out)
		git_buf_copy_cstr(tracking_branch_name_out, buffer_size, &buf);

	error = (int)buf.size + 1;

cleanup:
	git_buf_free(&buf);
	return (int)error;
}

int git_branch_tracking(
		git_reference **tracking_out,
		git_reference *branch)
{
	int error;
	git_buf tracking_name = GIT_BUF_INIT;

	if ((error = git_branch_tracking__name(&tracking_name,
		git_reference_owner(branch), git_reference_name(branch))) < 0)
			return error;

	error = git_reference_lookup(
		tracking_out,
		git_reference_owner(branch),
		git_buf_cstr(&tracking_name));

	git_buf_free(&tracking_name);
	return error;
}

int git_branch_is_head(
		git_reference *branch)
{
	git_reference *head;
	bool is_same = false;
	int error;

	assert(branch);

	if (!git_reference_is_branch(branch))
		return false;

	error = git_repository_head(&head, git_reference_owner(branch));

	if (error == GIT_EORPHANEDHEAD || error == GIT_ENOTFOUND)
		return false;

	if (error < 0)
		return -1;

	is_same = strcmp(
		git_reference_name(branch),
		git_reference_name(head)) == 0;

	git_reference_free(head);

	return is_same;
}
