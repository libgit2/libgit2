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
#include "status.h"

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

static bool status_compute_flags(
	unsigned int *out,
	git_diff_delta *h2i,
	git_diff_delta *i2w,
	const git_status_options *opts)
{
	unsigned int status = 0;

	if (h2i)
		status |= index_delta2status(h2i->status);

	if (i2w)
		status |= workdir_delta2status(i2w->status);

	if (opts->flags & GIT_STATUS_OPT_EXCLUDE_SUBMODULES) {
		bool in_tree  = (h2i && h2i->status != GIT_DELTA_ADDED);
		bool in_index = (h2i && h2i->status != GIT_DELTA_DELETED);
		bool in_wd    = (i2w && i2w->status != GIT_DELTA_DELETED);

		if ((!in_tree || h2i->old_file.mode == GIT_FILEMODE_COMMIT) &&
			(!in_index || h2i->new_file.mode == GIT_FILEMODE_COMMIT) &&
			(!in_wd || i2w->new_file.mode == GIT_FILEMODE_COMMIT))
			return 0;
	}

	*out = status;
	return 1;
}

typedef struct {
	git_status_cb cb;
	void *payload;
	const git_status_options *opts;
} status_user_callback;

typedef struct {
	git_diff_delta *head2idx;
	git_diff_delta *idx2wd;
} status_entry;

static void status_iterator_setup(
	git_status_iterator *it,
	const git_status_options *opts,
	git_diff_list *head2idx,
	git_diff_list *idx2wd)
{
	it->opts = opts;

	it->h2i = head2idx;
	it->h2i_idx = 0;
	it->h2i_len = head2idx ? head2idx->deltas.length : 0;

	it->i2w = idx2wd;
	it->i2w_idx = 0;
	it->i2w_len = idx2wd ? idx2wd->deltas.length : 0;

   /* Assert both iterators use matching ignore-case. If this function ever
    * supports merging diffs that are not sorted by the same function, then
    * it will need to spool and sort on one of the results before merging
    */
   if (head2idx && idx2wd) {
       assert(head2idx->strcomp == idx2wd->strcomp);
   }

   	it->strcomp = head2idx ? head2idx->strcomp : idx2wd ? idx2wd->strcomp : NULL;
}

int git_status_iterator_new_ext(
	git_status_iterator **out,
	git_repository *repo,
	const git_status_options *opts)
{
	int err = 0;
	git_status_iterator *it = NULL;
	git_diff_options diffopt = GIT_DIFF_OPTIONS_INIT;
	git_diff_list *head2idx = NULL, *idx2wd = NULL;
	git_tree *head = NULL;
	git_status_show_t show =
		opts ? opts->show : GIT_STATUS_SHOW_INDEX_AND_WORKDIR;

	assert(show <= GIT_STATUS_SHOW_INDEX_THEN_WORKDIR);

	GITERR_CHECK_VERSION(opts, GIT_STATUS_OPTIONS_VERSION, "git_status_options");

	it = git__calloc(1, sizeof(git_status_iterator));
	GITERR_CHECK_ALLOC(it);

	if (show != GIT_STATUS_SHOW_INDEX_ONLY &&
		(err = git_repository__ensure_not_bare(repo, "status")) < 0)
		goto on_error;

	/* if there is no HEAD, that's okay - we'll make an empty iterator */
	if (((err = git_repository_head_tree(&head, repo)) < 0) &&
		!(err == GIT_ENOTFOUND || err == GIT_EORPHANEDHEAD))
		goto on_error;

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
	if ((opts->flags & GIT_STATUS_OPT_RECURSE_IGNORED_DIRS) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_RECURSE_IGNORED_DIRS;
	if ((opts->flags & GIT_STATUS_OPT_EXCLUDE_SUBMODULES) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_IGNORE_SUBMODULES;

	if (show != GIT_STATUS_SHOW_WORKDIR_ONLY) {
		err = git_diff_tree_to_index(&head2idx, repo, head, NULL, &diffopt);
		if (err < 0)
			goto on_error;
	}

	if (show != GIT_STATUS_SHOW_INDEX_ONLY) {
		err = git_diff_index_to_workdir(&idx2wd, repo, NULL, &diffopt);
		if (err < 0)
			goto on_error;
	}

	status_iterator_setup(it, opts, head2idx, idx2wd);

	*out = it;

	git_tree_free(head);

	return 0;

on_error:
	git_diff_list_free(head2idx);
	git_diff_list_free(idx2wd);

	git_tree_free(head);

	git__free(it);

	return err;
}

static int status_next_index_then_workdir(
	const char **path_old_out,
	const char **path_new_out,
	unsigned int *status_out,
	git_status_iterator *it)
{
	git_diff_delta *delta;
	unsigned int status = 0;
	bool found = 0;

	while (!found) {
		if (it->h2i_idx < it->h2i_len) {
			delta = git_vector_get(&it->h2i->deltas, it->h2i_idx);
			it->h2i_idx++;

			found = status_compute_flags(&status, delta, NULL, it->opts);
		} else if (it->i2w_idx < it->i2w_len) {
			delta = git_vector_get(&it->i2w->deltas, it->i2w_idx);
			it->i2w_idx++;

			found = status_compute_flags(&status, NULL, delta, it->opts);
		} else
			return GIT_ITEROVER;
	}

	*path_old_out = delta->old_file.path;
	*path_new_out = delta->new_file.path;
	*status_out = status;

	return 0;
}

static int status_next_paired(
	const char **path_old_out,
	const char **path_new_out,
	unsigned int *status_out,
	git_status_iterator *it)
{
	git_diff_delta *h2i, *i2w;
	unsigned int status = 0;
	int cmp;
	bool found = 0;

	while (!found && (it->h2i_idx < it->h2i_len || it->i2w_idx < it->i2w_len)) {
		h2i = it->h2i_idx < it->h2i_len ?
			git_vector_get(&it->h2i->deltas, it->h2i_idx) : NULL;
		i2w = it->i2w_idx < it->i2w_len ?
			git_vector_get(&it->i2w->deltas, it->i2w_idx) : NULL;

		cmp = !i2w ? -1 : !h2i ? 1 :
			it->strcomp(h2i->old_file.path, i2w->old_file.path);

		if (cmp < 0) {
			it->h2i_idx++;
			i2w = NULL;
		} else if(cmp > 0) {
			h2i = NULL;
			it->i2w_idx++;
		} else {
			it->h2i_idx++;
			it->i2w_idx++;
		}

		if ((found = status_compute_flags(&status, h2i, i2w, it->opts)) == 1) {
			*path_old_out = h2i ? h2i->old_file.path : i2w->old_file.path;
			*path_new_out = i2w ? i2w->new_file.path : h2i->new_file.path;
			*status_out = status;

			return 0;
		}
	}

	return GIT_ITEROVER;
}

int git_status_next(
	const char **path_old,
	const char **path_new,
	unsigned int *status,
	git_status_iterator *it)
{
	assert(path_old && path_new && status && it);

	*path_old = NULL;
	*path_new = NULL;
	*status = 0;

	return (it->opts->show == GIT_STATUS_SHOW_INDEX_THEN_WORKDIR) ?
		status_next_index_then_workdir(path_old, path_new, status, it) :
		status_next_paired(path_old, path_new, status, it);
}

void git_status_iterator_free(git_status_iterator *it)
{
	if (it == NULL)
		return;

	git_diff_list_free(it->h2i);
	git_diff_list_free(it->i2w);

	git__free(it);
}

int git_status_foreach_ext(
	git_repository *repo,
	const git_status_options *opts,
	git_status_cb callback,
	void *payload)
{
	git_status_iterator *it;
	const char *path_old, *path_new;
	unsigned int status;
	int error = 0;

	if ((error = git_status_iterator_new_ext(&it, repo, opts)) < 0)
		return error;

	while ((error = git_status_next(&path_old, &path_new, &status, it)) == 0) {
		if (callback(path_old, status, payload) != 0) {
			error = GIT_EUSER;
			break;
		}
	}

	if (error == GIT_ITEROVER)
		error = 0;

	git_status_iterator_free(it);

	return error;
}

#define GIT_STATUS_OPTIONS_DEFAULT { \
	GIT_STATUS_OPTIONS_VERSION, \
	GIT_STATUS_SHOW_INDEX_AND_WORKDIR, \
	GIT_STATUS_OPT_INCLUDE_IGNORED | GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS \
}

int git_status_iterator_new(
	git_status_iterator **it,
	git_repository *repo)
{
	git_status_options opts = GIT_STATUS_OPTIONS_DEFAULT;

	return git_status_iterator_new_ext(it, repo, &opts);
}

int git_status_foreach(
	git_repository *repo,
	git_status_cb callback,
	void *payload)
{
	git_status_options opts = GIT_STATUS_OPTIONS_DEFAULT;

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
		return GIT_EAMBIGUOUS; /* giterr_set will be done by caller */
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
		GIT_STATUS_OPT_RECURSE_IGNORED_DIRS |
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
		giterr_set(GITERR_INVALID,
			"Attempt to get status of nonexistent file '%s'", path);
		error = GIT_ENOTFOUND;
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

