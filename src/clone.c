/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#ifndef GIT_WIN32
#include <dirent.h>
#endif

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

GIT_BEGIN_DECL

struct HeadInfo {
	git_repository *repo;
	git_oid remote_head_oid;
	git_buf branchname;
};

static int create_tracking_branch(git_repository *repo, const git_oid *target, const char *name)
{
	git_object *head_obj = NULL;
	git_reference *branch_ref;
	int retcode = GIT_ERROR;

	/* Find the target commit */
	if (git_object_lookup(&head_obj, repo, target, GIT_OBJ_ANY) < 0)
		return GIT_ERROR;

	/* Create the new branch */
	if (!git_branch_create(&branch_ref, repo, name, head_obj, 0)) {
		git_config *cfg;

		git_reference_free(branch_ref);
		/* Set up tracking */
		if (!git_repository_config(&cfg, repo)) {
			git_buf remote = GIT_BUF_INIT;
			git_buf merge = GIT_BUF_INIT;
			git_buf merge_target = GIT_BUF_INIT;
			if (!git_buf_printf(&remote, "branch.%s.remote", name) &&
				 !git_buf_printf(&merge, "branch.%s.merge", name) &&
				 !git_buf_printf(&merge_target, "refs/heads/%s", name) &&
				 !git_config_set_string(cfg, git_buf_cstr(&remote), "origin") &&
				 !git_config_set_string(cfg, git_buf_cstr(&merge), git_buf_cstr(&merge_target))) {
				retcode = 0;
			}
			git_buf_free(&remote);
			git_buf_free(&merge);
			git_buf_free(&merge_target);
			git_config_free(cfg);
		}
	}

	git_object_free(head_obj);
	return retcode;
}

static int reference_matches_remote_head(const char *head_name, void *payload)
{
	struct HeadInfo *head_info = (struct HeadInfo *)payload;
	git_oid oid;

	/* Stop looking if we've already found a match */
	if (git_buf_len(&head_info->branchname) > 0) return 0;

	if (!git_reference_name_to_oid(&oid, head_info->repo, head_name) &&
		 !git_oid_cmp(&head_info->remote_head_oid, &oid)) {
		git_buf_puts(&head_info->branchname,
						 head_name+strlen("refs/remotes/origin/"));
	}
	return 0;
}

static int update_head_to_new_branch(git_repository *repo, const git_oid *target, const char *name)
{
	int retcode = GIT_ERROR;

	if (!create_tracking_branch(repo, target, name)) {
		git_reference *head;
		if (!git_reference_lookup(&head, repo, GIT_HEAD_FILE)) {
			git_buf targetbuf = GIT_BUF_INIT;
			if (!git_buf_printf(&targetbuf, "refs/heads/%s", name)) {
				retcode = git_reference_set_target(head, git_buf_cstr(&targetbuf));
			}
			git_buf_free(&targetbuf);
			git_reference_free(head);
		}
	}

	return retcode;
}

static int update_head_to_remote(git_repository *repo, git_remote *remote)
{
	int retcode = GIT_ERROR;
	git_remote_head *remote_head;
	git_oid oid;
	struct HeadInfo head_info;

	/* Get the remote's HEAD. This is always the first ref in remote->refs. */
	remote_head = remote->refs.contents[0];
	git_oid_cpy(&head_info.remote_head_oid, &remote_head->oid);
	git_buf_init(&head_info.branchname, 16);
	head_info.repo = repo;

	/* Check to see if "master" matches the remote head */
	if (!git_reference_name_to_oid(&oid, repo, "refs/remotes/origin/master") &&
		 !git_oid_cmp(&remote_head->oid, &oid)) {
		retcode = update_head_to_new_branch(repo, &oid, "master");
	}
	/* Not master. Check all the other refs. */
	else if (!git_reference_foreach(repo, GIT_REF_LISTALL,
												 reference_matches_remote_head,
												 &head_info) &&
				git_buf_len(&head_info.branchname) > 0) {
		retcode = update_head_to_new_branch(repo, &head_info.remote_head_oid,
														git_buf_cstr(&head_info.branchname));
	}

	git_buf_free(&head_info.branchname);
	return retcode;
}

/*
 * submodules?
 */



static int setup_remotes_and_fetch(git_repository *repo,
											  const char *origin_url,
											  git_indexer_stats *fetch_stats)
{
	int retcode = GIT_ERROR;
	git_remote *origin = NULL;
	git_off_t bytes = 0;
	git_indexer_stats dummy_stats;

	if (!fetch_stats) fetch_stats = &dummy_stats;

	/* Create the "origin" remote */
	if (!git_remote_add(&origin, repo, "origin", origin_url)) {
		/* Connect and download everything */
		if (!git_remote_connect(origin, GIT_DIR_FETCH)) {
			if (!git_remote_download(origin, &bytes, fetch_stats)) {
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


static int clone_internal(git_repository **out,
								  const char *origin_url,
								  const char *path,
								  git_indexer_stats *fetch_stats,
								  int is_bare)
{
	int retcode = GIT_ERROR;
	git_repository *repo = NULL;
	git_indexer_stats dummy_stats;

	if (!fetch_stats) fetch_stats = &dummy_stats;

	if (!path_is_okay(path)) {
		return GIT_ERROR;
	}

	if (!(retcode = git_repository_init(&repo, path, is_bare))) {
		if ((retcode = setup_remotes_and_fetch(repo, origin_url, fetch_stats)) < 0) {
			/* Failed to fetch; clean up */
			git_repository_free(repo);
			git_futils_rmdir_r(path, GIT_DIRREMOVAL_FILES_AND_DIRS);
		} else {
			*out = repo;
			retcode = 0;
		}
	}

	return retcode;
}

int git_clone_bare(git_repository **out,
						 const char *origin_url,
						 const char *dest_path,
						 git_indexer_stats *fetch_stats)
{
	assert(out && origin_url && dest_path);
	return clone_internal(out, origin_url, dest_path, fetch_stats, 1);
}


int git_clone(git_repository **out,
				  const char *origin_url,
				  const char *workdir_path,
				  git_indexer_stats *fetch_stats,
				  git_indexer_stats *checkout_stats,
				  git_checkout_opts *checkout_opts)
{
	int retcode = GIT_ERROR;

	assert(out && origin_url && workdir_path);

	if (!(retcode = clone_internal(out, origin_url, workdir_path, fetch_stats, 0))) {
		retcode = git_checkout_head(*out, checkout_opts, checkout_stats);
	}

	return retcode;
}




GIT_END_DECL
