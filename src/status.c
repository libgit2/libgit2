/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2.h"
#include "fileops.h"
#include "hash.h"
#include "vector.h"
#include "tree.h"
#include "git2/status.h"
#include "repository.h"
#include "ignore.h"

#include "git2/diff.h"
#include "diff.h"
#include "diff_output.h"

static unsigned int index_delta2status(git_delta_t index_status)
{
	unsigned int st = GIT_STATUS_CURRENT;

	switch (index_status) {
	case GIT_DELTA_ADDED:
	case GIT_DELTA_COPIED:
		st = GIT_STATUS_INDEX_NEW;
		break;
	case GIT_DELTA_DELETED:
		st = GIT_STATUS_INDEX_DELETED;
		break;
	case GIT_DELTA_MODIFIED:
		st = GIT_STATUS_INDEX_MODIFIED;
		break;
	case GIT_DELTA_RENAMED:
		st = GIT_STATUS_INDEX_RENAMED;
		break;
	case GIT_DELTA_TYPECHANGE:
		st = GIT_STATUS_INDEX_TYPECHANGE;
		break;
	default:
		break;
	}

	return st;
}

static unsigned int workdir_delta2status(git_delta_t workdir_status)
{
	unsigned int st = GIT_STATUS_CURRENT;

	switch (workdir_status) {
	case GIT_DELTA_ADDED:
	case GIT_DELTA_RENAMED:
	case GIT_DELTA_COPIED:
	case GIT_DELTA_UNTRACKED:
		st = GIT_STATUS_WT_NEW;
		break;
	case GIT_DELTA_DELETED:
		st = GIT_STATUS_WT_DELETED;
		break;
	case GIT_DELTA_MODIFIED:
		st = GIT_STATUS_WT_MODIFIED;
		break;
	case GIT_DELTA_IGNORED:
		st = GIT_STATUS_IGNORED;
		break;
	case GIT_DELTA_TYPECHANGE:
		st = GIT_STATUS_WT_TYPECHANGE;
		break;
	default:
		break;
	}

	return st;
}

typedef struct {
	git_status_cb cb;
	void *payload;
} status_user_callback;

static int status_invoke_cb(
	git_diff_delta *i2h, git_diff_delta *w2i, void *payload)
{
	status_user_callback *usercb = payload;
	const char *path = NULL;
	unsigned int status = 0;

	if (w2i) {
		path = w2i->old_file.path;
		status |= workdir_delta2status(w2i->status);
	}
	if (i2h) {
		path = i2h->old_file.path;
		status |= index_delta2status(i2h->status);
	}

	return usercb->cb(path, status, usercb->payload);
}

int git_status_foreach_ext(
	git_repository *repo,
	const git_status_options *opts,
	git_status_cb cb,
	void *payload)
{
	int err = 0;
	git_diff_options diffopt = GIT_DIFF_OPTIONS_INIT;
	git_diff_list *idx2head = NULL, *wd2idx = NULL;
	git_tree *head = NULL;
	git_status_show_t show =
		opts ? opts->show : GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	status_user_callback usercb;

	assert(show <= GIT_STATUS_SHOW_INDEX_THEN_WORKDIR);

	GITERR_CHECK_VERSION(opts, GIT_STATUS_OPTIONS_VERSION, "git_status_options");

	if (show != GIT_STATUS_SHOW_INDEX_ONLY &&
		(err = git_repository__ensure_not_bare(repo, "status")) < 0)
		return err;

	/* if there is no HEAD, that's okay - we'll make an empty iterator */
	if (((err = git_repository_head_tree(&head, repo)) < 0) &&
		!(err == GIT_ENOTFOUND || err == GIT_EORPHANEDHEAD))
			return err;

	memcpy(&diffopt.pathspec, &opts->pathspec, sizeof(diffopt.pathspec));

	diffopt.flags = GIT_DIFF_INCLUDE_TYPECHANGE;

	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_UNTRACKED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_UNTRACKED;
	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_IGNORED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_IGNORED;
	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_UNMODIFIED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_UNMODIFIED;
	if ((opts->flags & GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_RECURSE_UNTRACKED_DIRS;
	if ((opts->flags & GIT_STATUS_OPT_DISABLE_PATHSPEC_MATCH) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_DISABLE_PATHSPEC_MATCH;
	/* TODO: support EXCLUDE_SUBMODULES flag */

	if (show != GIT_STATUS_SHOW_WORKDIR_ONLY &&
		(err = git_diff_tree_to_index(&idx2head, repo, head, NULL, &diffopt)) < 0)
		goto cleanup;

	if (show != GIT_STATUS_SHOW_INDEX_ONLY &&
		(err = git_diff_index_to_workdir(&wd2idx, repo, NULL, &diffopt)) < 0)
		goto cleanup;

	usercb.cb = cb;
	usercb.payload = payload;

	if (show == GIT_STATUS_SHOW_INDEX_THEN_WORKDIR) {
		if ((err = git_diff__paired_foreach(
				 idx2head, NULL, status_invoke_cb, &usercb)) < 0)
			goto cleanup;

		git_diff_list_free(idx2head);
		idx2head = NULL;
	}

	err = git_diff__paired_foreach(idx2head, wd2idx, status_invoke_cb, &usercb);

cleanup:
	git_tree_free(head);
	git_diff_list_free(idx2head);
	git_diff_list_free(wd2idx);

	if (err == GIT_EUSER)
		giterr_clear();

	return err;
}

int git_status_foreach(
	git_repository *repo,
	git_status_cb callback,
	void *payload)
{
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;

	opts.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	opts.flags = GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;

	return git_status_foreach_ext(repo, &opts, callback, payload);
}

struct status_file_info {
	char *expected;
	unsigned int count;
	unsigned int status;
	int fnm_flags;
	int ambiguous;
};

static int get_one_status(const char *path, unsigned int status, void *data)
{
	struct status_file_info *sfi = data;
	int (*strcomp)(const char *a, const char *b);

	sfi->count++;
	sfi->status = status;

	strcomp = (sfi->fnm_flags & FNM_CASEFOLD) ? git__strcasecmp : git__strcmp;

	if (sfi->count > 1 ||
		(strcomp(sfi->expected, path) != 0 &&
		 p_fnmatch(sfi->expected, path, sfi->fnm_flags) != 0))
	{
		sfi->ambiguous = true;
		return GIT_EAMBIGUOUS;
	}

	return 0;
}

int git_status_file(
	unsigned int *status_flags,
	git_repository *repo,
	const char *path)
{
	int error;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_file_info sfi = {0};
	git_index *index;

	assert(status_flags && repo && path);

	if ((error = git_repository_index__weakptr(&index, repo)) < 0)
		return error;

	if ((sfi.expected = git__strdup(path)) == NULL)
		return -1;
	if (index->ignore_case)
		sfi.fnm_flags = FNM_CASEFOLD;

	opts.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	opts.flags = GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
		GIT_STATUS_OPT_INCLUDE_UNMODIFIED;
	opts.pathspec.count = 1;
	opts.pathspec.strings = &sfi.expected;

	error = git_status_foreach_ext(repo, &opts, get_one_status, &sfi);

	if (error < 0 && sfi.ambiguous) {
		giterr_set(GITERR_INVALID,
			"Ambiguous path '%s' given to git_status_file", sfi.expected);
		error = GIT_EAMBIGUOUS;
	}

	if (!error && !sfi.count) {
		git_buf full = GIT_BUF_INIT;

		/* if the file actually exists and we still did not get a callback
		 * for it, then it must be contained inside an ignored directory, so
		 * mark it as such instead of generating an error.
		 */
		if (!git_buf_joinpath(&full, git_repository_workdir(repo), path) &&
			git_path_exists(full.ptr))
			sfi.status = GIT_STATUS_IGNORED;
		else {
			giterr_set(GITERR_INVALID,
				"Attempt to get status of nonexistent file '%s'", path);
			error = GIT_ENOTFOUND;
		}

		git_buf_free(&full);
	}

	*status_flags = sfi.status;

	git__free(sfi.expected);

	return error;
}

int git_status_should_ignore(
	int *ignored,
	git_repository *repo,
	const char *path)
{
	return git_ignore_path_is_ignored(ignored, repo, path);
}

