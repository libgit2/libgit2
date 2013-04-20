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
#include "remote.h"

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
	git_branch_foreach_cb branch_cb;
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
	git_branch_foreach_cb branch_cb,
	void *payload)
{
	branch_foreach_filter filter;

	filter.branch_cb = branch_cb;
	filter.branch_type = list_flags;
	filter.callback_payload = payload;

	return git_reference_foreach(repo, GIT_REF_LISTALL, &branch_foreach_cb, (void *)&filter);
}

int git_branch_move(
	git_reference **out,
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

	if ((error = git_buf_joinpath(&new_reference_name, GIT_REFS_HEADS_DIR, new_branch_name)) < 0 ||
		(error = git_buf_printf(&old_config_section, "branch.%s", git_reference_name(branch) + strlen(GIT_REFS_HEADS_DIR))) < 0 ||
		(error = git_buf_printf(&new_config_section, "branch.%s", new_branch_name)) < 0)
		goto done;

	if ((error = git_config_rename_section(git_reference_owner(branch),
		git_buf_cstr(&old_config_section),
		git_buf_cstr(&new_config_section))) < 0)
		goto done;
	
	if ((error = git_reference_rename(out, branch, git_buf_cstr(&new_reference_name), force)) < 0)
		goto done;

done:
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

static int retrieve_upstream_configuration(
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

int git_branch_upstream__name(
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

	if ((error = retrieve_upstream_configuration(
		&remote_name, repo, canonical_branch_name, "branch.%s.remote")) < 0)
			goto cleanup;

	if ((error = retrieve_upstream_configuration(
		&merge_name, repo, canonical_branch_name, "branch.%s.merge")) < 0)
			goto cleanup;

	if (!*remote_name || !*merge_name) {
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	if (strcmp(".", remote_name) != 0) {
		if ((error = git_remote_load(&remote, repo, remote_name)) < 0)
			goto cleanup;

		refspec = git_remote__matching_refspec(remote, merge_name);
		if (!refspec) {
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

static int remote_name(git_buf *buf, git_repository *repo, const char *canonical_branch_name)
{
	git_strarray remote_list = {0};
	size_t i;
	git_remote *remote;
	const git_refspec *fetchspec;
	int error = 0;
	char *remote_name = NULL;

	assert(buf && repo && canonical_branch_name);

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
			continue;

		fetchspec = git_remote__matching_dst_refspec(remote, canonical_branch_name);
		if (fetchspec) {
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
		git_buf_clear(buf);
		error = git_buf_puts(buf, remote_name);
	} else {
		error = GIT_ENOTFOUND;
	}

cleanup:
	git_strarray_free(&remote_list);
	return error;
}

int git_branch_remote_name(char *buffer, size_t buffer_len, git_repository *repo, const char *refname)
{
	int ret;
	git_buf buf = GIT_BUF_INIT;

	if ((ret = remote_name(&buf, repo, refname)) < 0)
		return ret;

	if (buffer)
		git_buf_copy_cstr(buffer, buffer_len, &buf);

	ret = (int)git_buf_len(&buf) + 1;
	git_buf_free(&buf);

	return ret;
}

int git_branch_upstream_name(
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

	if ((error = git_branch_upstream__name(
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

int git_branch_upstream(
		git_reference **tracking_out,
		git_reference *branch)
{
	int error;
	git_buf tracking_name = GIT_BUF_INIT;

	if ((error = git_branch_upstream__name(&tracking_name,
		git_reference_owner(branch), git_reference_name(branch))) < 0)
			return error;

	error = git_reference_lookup(
		tracking_out,
		git_reference_owner(branch),
		git_buf_cstr(&tracking_name));

	git_buf_free(&tracking_name);
	return error;
}

static int unset_upstream(git_config *config, const char *shortname)
{
	git_buf buf = GIT_BUF_INIT;

	if (git_buf_printf(&buf, "branch.%s.remote", shortname) < 0)
		return -1;

	if (git_config_delete_entry(config, git_buf_cstr(&buf)) < 0)
		goto on_error;

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "branch.%s.merge", shortname) < 0)
		goto on_error;

	if (git_config_delete_entry(config, git_buf_cstr(&buf)) < 0)
		goto on_error;

	git_buf_free(&buf);
	return 0;

on_error:
	git_buf_free(&buf);
	return -1;
}

int git_branch_set_upstream(git_reference *branch, const char *upstream_name)
{
	git_buf key = GIT_BUF_INIT, value = GIT_BUF_INIT;
	git_reference *upstream;
	git_repository *repo;
	git_remote *remote = NULL;
	git_config *config;
	const char *name, *shortname;
	int local;
	const git_refspec *fetchspec;

	name = git_reference_name(branch);
	if (!git_reference__is_branch(name))
		return not_a_local_branch(name);

	if (git_repository_config__weakptr(&config, git_reference_owner(branch)) < 0)
		return -1;

	shortname = name + strlen(GIT_REFS_HEADS_DIR);

	if (upstream_name == NULL)
		return unset_upstream(config, shortname);

	repo = git_reference_owner(branch);

	/* First we need to figure out whether it's a branch or remote-tracking */
	if (git_branch_lookup(&upstream, repo, upstream_name, GIT_BRANCH_LOCAL) == 0)
		local = 1;
	else if (git_branch_lookup(&upstream, repo, upstream_name, GIT_BRANCH_REMOTE) == 0)
		local = 0;
	else
		return GIT_ENOTFOUND;

	/*
	 * If it's local, the remote is "." and the branch name is
	 * simply the refname. Otherwise we need to figure out what
	 * the remote-tracking branch's name on the remote is and use
	 * that.
	 */
	if (local)
		git_buf_puts(&value, ".");
	else
		remote_name(&value, repo, git_reference_name(upstream));

	if (git_buf_printf(&key, "branch.%s.remote", shortname) < 0)
		goto on_error;

	if (git_config_set_string(config, git_buf_cstr(&key), git_buf_cstr(&value)) < 0)
		goto on_error;

	if (local) {
		if (git_buf_puts(&value, git_reference_name(branch)) < 0)
			goto on_error;
	} else {
		/* Get the remoe-tracking branch's refname in its repo */
		if (git_remote_load(&remote, repo, git_buf_cstr(&value)) < 0)
			goto on_error;

		fetchspec = git_remote__matching_dst_refspec(remote, git_reference_name(upstream));
		git_buf_clear(&value);
		if (!fetchspec || git_refspec_transform_l(&value, fetchspec, git_reference_name(upstream)) < 0)
			goto on_error;

		git_remote_free(remote);
		remote = NULL;
	}

	git_buf_clear(&key);
	if (git_buf_printf(&key, "branch.%s.merge", shortname) < 0)
		goto on_error;

	if (git_config_set_string(config, git_buf_cstr(&key), git_buf_cstr(&value)) < 0)
		goto on_error;

	git_reference_free(upstream);
	git_buf_free(&key);
	git_buf_free(&value);

	return 0;

on_error:
	git_reference_free(upstream);
	git_buf_free(&key);
	git_buf_free(&value);
	git_remote_free(remote);

	return -1;
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
