/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "git2/clone.h"
#include "git2/remote.h"
#include "git2/revparse.h"
#include "git2/branch.h"
#include "git2/config.h"
#include "git2/checkout.h"
#include "git2/commit.h"
#include "git2/tree.h"

#include "common.h"
#include "remote.h"
#include "fileops.h"
#include "refs.h"
#include "path.h"

static int create_branch(
	git_reference **branch,
	git_repository *repo,
	const git_oid *target,
	const char *name)
{
	git_commit *head_obj = NULL;
	git_reference *branch_ref;
	int error;

	/* Find the target commit */
	if ((error = git_commit_lookup(&head_obj, repo, target)) < 0)
		return error;

	/* Create the new branch */
	error = git_branch_create(&branch_ref, repo, name, head_obj, 0);

	git_commit_free(head_obj);

	if (!error)
		*branch = branch_ref;
	else
		git_reference_free(branch_ref);

	return error;
}

static int setup_tracking_config(
	git_repository *repo,
	const char *branch_name,
	const char *remote_name,
	const char *merge_target)
{
	git_config *cfg;
	git_buf remote_key = GIT_BUF_INIT, merge_key = GIT_BUF_INIT;
	int error = -1;

	if (git_repository_config__weakptr(&cfg, repo) < 0)
		return -1;

	if (git_buf_printf(&remote_key, "branch.%s.remote", branch_name) < 0)
		goto cleanup;

	if (git_buf_printf(&merge_key, "branch.%s.merge", branch_name) < 0)
		goto cleanup;

	if (git_config_set_string(cfg, git_buf_cstr(&remote_key), remote_name) < 0)
		goto cleanup;

	if (git_config_set_string(cfg, git_buf_cstr(&merge_key), merge_target) < 0)
		goto cleanup;

	error = 0;

cleanup:
	git_buf_free(&remote_key);
	git_buf_free(&merge_key);
	return error;
}

static int create_tracking_branch(
	git_reference **branch,
	git_repository *repo,
	const git_oid *target,
	const char *branch_name)
{
	int error;

	if ((error = create_branch(branch, repo, target, branch_name)) < 0)
		return error;

	return setup_tracking_config(
		repo,
		branch_name,
		GIT_REMOTE_ORIGIN,
		git_reference_name(*branch));
}

struct head_info {
	git_repository *repo;
	git_oid remote_head_oid;
	git_buf branchname;
	const git_refspec *refspec;
};

static int reference_matches_remote_head(
	const char *reference_name,
	void *payload)
{
	struct head_info *head_info = (struct head_info *)payload;
	git_oid oid;

	/* TODO: Should we guard against references
	 * which name doesn't start with refs/heads/ ?
	 */

	/* Stop looking if we've already found a match */
	if (git_buf_len(&head_info->branchname) > 0)
		return 0;

	if (git_reference_name_to_id(
		&oid,
		head_info->repo,
		reference_name) < 0) {
			/* TODO: How to handle not found references?
			 */
			return -1;
	}

	if (git_oid_cmp(&head_info->remote_head_oid, &oid) == 0) {
		/* Determine the local reference name from the remote tracking one */
		if (git_refspec_transform_l(
			&head_info->branchname, 
			head_info->refspec,
			reference_name) < 0)
				return -1;
		
		if (git_buf_sets(
			&head_info->branchname,
			git_buf_cstr(&head_info->branchname) + strlen(GIT_REFS_HEADS_DIR)) < 0)
				return -1;
	}

	return 0;
}

static int update_head_to_new_branch(
	git_repository *repo,
	const git_oid *target,
	const char *name)
{
	git_reference *tracking_branch = NULL;
	int error;

	if ((error = create_tracking_branch(
		&tracking_branch,
		repo,
		target,
		name)) < 0)
			return error;

	error = git_repository_set_head(repo, git_reference_name(tracking_branch));

	git_reference_free(tracking_branch);

	return error;
}

static int get_head_callback(git_remote_head *head, void *payload)
{
	git_remote_head **destination = (git_remote_head **)payload;

	/* Save the first entry, and terminate the enumeration */
	*destination = head;
	return 1;
}

static int update_head_to_remote(git_repository *repo, git_remote *remote)
{
	int retcode = -1;
	git_remote_head *remote_head;
	struct head_info head_info;
	git_buf remote_master_name = GIT_BUF_INIT;

	/* Did we just clone an empty repository? */
	if (remote->refs.length == 0) {
		return setup_tracking_config(
			repo,
			"master",
			GIT_REMOTE_ORIGIN,
			GIT_REFS_HEADS_MASTER_FILE);
	}

	/* Get the remote's HEAD. This is always the first ref in remote->refs. */
	remote_head = NULL;
	
	if (!remote->transport->ls(remote->transport, get_head_callback, &remote_head))
		return -1;

	assert(remote_head);

	git_oid_cpy(&head_info.remote_head_oid, &remote_head->oid);
	git_buf_init(&head_info.branchname, 16);
	head_info.repo = repo;
	head_info.refspec = git_remote_fetchspec(remote);
	
	/* Determine the remote tracking reference name from the local master */
	if (git_refspec_transform_r(
		&remote_master_name,
		head_info.refspec,
		GIT_REFS_HEADS_MASTER_FILE) < 0)
			return -1;

	/* Check to see if the remote HEAD points to the remote master */
	if (reference_matches_remote_head(git_buf_cstr(&remote_master_name), &head_info) < 0)
		goto cleanup;

	if (git_buf_len(&head_info.branchname) > 0) {
		retcode = update_head_to_new_branch(
			repo,
			&head_info.remote_head_oid,
			git_buf_cstr(&head_info.branchname));

		goto cleanup;
	}

	/* Not master. Check all the other refs. */
	if (git_reference_foreach(
		repo,
		GIT_REF_LISTALL,
		reference_matches_remote_head,
		&head_info) < 0)
			goto cleanup;

	if (git_buf_len(&head_info.branchname) > 0) {
		retcode = update_head_to_new_branch(
			repo,
			&head_info.remote_head_oid,
			git_buf_cstr(&head_info.branchname));

		goto cleanup;
	} else {
		/* TODO: What should we do if nothing has been found?
		 */
	}

cleanup:
	git_buf_free(&remote_master_name);
	git_buf_free(&head_info.branchname);
	return retcode;
}

/*
 * submodules?
 */



static int setup_remotes_and_fetch(
		git_repository *repo,
		git_remote *origin,
		git_transfer_progress_callback progress_cb,
		void *progress_payload)
{
	int retcode = GIT_ERROR;

	/* Add the origin remote */
	if (!git_remote_set_repository(origin, repo) && !git_remote_save(origin)) {
		/*
		 * Don't write FETCH_HEAD, we'll check out the remote tracking
		 * branch ourselves based on the server's default.
		 */
		git_remote_set_update_fetchhead(origin, 0);

		/* Connect and download everything */
		if (!git_remote_connect(origin, GIT_DIRECTION_FETCH)) {
			if (!git_remote_download(origin, progress_cb, progress_payload)) {
				/* Create "origin/foo" branches for all remote branches */
				if (!git_remote_update_tips(origin)) {
					/* Point HEAD to the same ref as the remote's head */
					if (!update_head_to_remote(repo, origin)) {
						retcode = 0;
					}
				}
			}
			git_remote_disconnect(origin);
		}
		git_remote_free(origin);
	}

	return retcode;
}


static bool path_is_okay(const char *path)
{
	/* The path must either not exist, or be an empty directory */
	if (!git_path_exists(path)) return true;
	if (!git_path_is_empty_dir(path)) {
		giterr_set(GITERR_INVALID,
					  "'%s' exists and is not an empty directory", path);
		return false;
	}
	return true;
}

static bool should_checkout(
	git_repository *repo,
	bool is_bare,
	git_checkout_opts *opts)
{
	if (is_bare)
		return false;

	if (!opts)
		return false;

	return !git_repository_head_orphan(repo);
}

static int clone_internal(
	git_repository **out,
	git_remote *origin_remote,
	const char *path,
	git_transfer_progress_callback fetch_progress_cb,
	void *fetch_progress_payload,
	git_checkout_opts *checkout_opts,
	bool is_bare)
{
	int retcode = GIT_ERROR;
	git_repository *repo = NULL;

	if (!path_is_okay(path)) {
		return GIT_ERROR;
	}

	if (!(retcode = git_repository_init(&repo, path, is_bare))) {
		if ((retcode = setup_remotes_and_fetch(repo, origin_remote,
						fetch_progress_cb, fetch_progress_payload)) < 0) {
			/* Failed to fetch; clean up */
			git_repository_free(repo);
			git_futils_rmdir_r(path, NULL, GIT_RMDIR_REMOVE_FILES);
		} else {
			*out = repo;
			retcode = 0;
		}
	}

	if (!retcode && should_checkout(repo, is_bare, checkout_opts))
		retcode = git_checkout_head(*out, checkout_opts);

	return retcode;
}

int git_clone_bare(
		git_repository **out,
		git_remote *origin_remote,
		const char *dest_path,
		git_transfer_progress_callback fetch_progress_cb,
		void *fetch_progress_payload)
{
	assert(out && origin_remote && dest_path);

	return clone_internal(
		out,
		origin_remote,
		dest_path,
		fetch_progress_cb,
		fetch_progress_payload,
		NULL,
		1);
}


int git_clone(
		git_repository **out,
		git_remote *origin_remote,
		const char *workdir_path,
		git_checkout_opts *checkout_opts,
		git_transfer_progress_callback fetch_progress_cb,
		void *fetch_progress_payload)
{
	assert(out && origin_remote && workdir_path);

	return clone_internal(
		out,
		origin_remote,
		workdir_path,
		fetch_progress_cb,
		fetch_progress_payload,
		checkout_opts,
		0);
}
