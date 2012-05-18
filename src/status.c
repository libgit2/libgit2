/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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

static unsigned int index_delta2status(git_delta_t index_status)
{
	unsigned int st = GIT_STATUS_CURRENT;

	switch (index_status) {
	case GIT_DELTA_ADDED:
	case GIT_DELTA_COPIED:
	case GIT_DELTA_RENAMED:
		st = GIT_STATUS_INDEX_NEW;
		break;
	case GIT_DELTA_DELETED:
		st = GIT_STATUS_INDEX_DELETED;
		break;
	case GIT_DELTA_MODIFIED:
		st = GIT_STATUS_INDEX_MODIFIED;
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
	case GIT_DELTA_COPIED:
	case GIT_DELTA_RENAMED:
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
	default:
		break;
	}

	return st;
}

int git_status_foreach_ext(
	git_repository *repo,
	const git_status_options *opts,
	int (*cb)(const char *, unsigned int, void *),
	void *cbdata)
{
	int err = 0, cmp;
	git_diff_options diffopt;
	git_diff_list *idx2head = NULL, *wd2idx = NULL;
	git_tree *head = NULL;
	git_status_show_t show =
		opts ? opts->show : GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	git_diff_delta *i2h, *w2i;
	unsigned int i, j, i_max, j_max;

	assert(show <= GIT_STATUS_SHOW_INDEX_THEN_WORKDIR);

	if ((err = git_repository_head_tree(&head, repo)) < 0)
		return err;

	memset(&diffopt, 0, sizeof(diffopt));
	memcpy(&diffopt.pathspec, &opts->pathspec, sizeof(diffopt.pathspec));

	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_UNTRACKED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_UNTRACKED;
	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_IGNORED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_IGNORED;
	if ((opts->flags & GIT_STATUS_OPT_INCLUDE_UNMODIFIED) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_INCLUDE_UNMODIFIED;
	if ((opts->flags & GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS) != 0)
		diffopt.flags = diffopt.flags | GIT_DIFF_RECURSE_UNTRACKED_DIRS;
	/* TODO: support EXCLUDE_SUBMODULES flag */

	if (show != GIT_STATUS_SHOW_WORKDIR_ONLY &&
		(err = git_diff_index_to_tree(repo, &diffopt, head, &idx2head)) < 0)
		goto cleanup;

	if (show != GIT_STATUS_SHOW_INDEX_ONLY &&
		(err = git_diff_workdir_to_index(repo, &diffopt, &wd2idx)) < 0)
		goto cleanup;

	if (show == GIT_STATUS_SHOW_INDEX_THEN_WORKDIR) {
		for (i = 0; !err && i < idx2head->deltas.length; i++) {
			i2h = GIT_VECTOR_GET(&idx2head->deltas, i);
			err = cb(i2h->old_file.path, index_delta2status(i2h->status), cbdata);
		}
		git_diff_list_free(idx2head);
		idx2head = NULL;
	}

	i_max = idx2head ? idx2head->deltas.length : 0;
	j_max = wd2idx   ? wd2idx->deltas.length   : 0;

	for (i = 0, j = 0; !err && (i < i_max || j < j_max); ) {
		i2h = idx2head ? GIT_VECTOR_GET(&idx2head->deltas,i) : NULL;
		w2i = wd2idx   ? GIT_VECTOR_GET(&wd2idx->deltas,j)   : NULL;

		cmp = !w2i ? -1 : !i2h ? 1 : strcmp(i2h->old_file.path, w2i->old_file.path);

		if (cmp < 0) {
			err = cb(i2h->old_file.path, index_delta2status(i2h->status), cbdata);
			i++;
		} else if (cmp > 0) {
			err = cb(w2i->old_file.path, workdir_delta2status(w2i->status), cbdata);
			j++;
		} else {
			err = cb(i2h->old_file.path, index_delta2status(i2h->status) |
					 workdir_delta2status(w2i->status), cbdata);
			i++; j++;
		}
	}

cleanup:
	git_tree_free(head);
	git_diff_list_free(idx2head);
	git_diff_list_free(wd2idx);
	return err;
}

int git_status_foreach(
	git_repository *repo,
	int (*callback)(const char *, unsigned int, void *),
	void *payload)
{
	git_status_options opts;

	memset(&opts, 0, sizeof(opts));
	opts.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	opts.flags = GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;

	return git_status_foreach_ext(repo, &opts, callback, payload);
}

struct status_file_info {
	unsigned int count;
	unsigned int status;
	char *expected;
};

static int get_one_status(const char *path, unsigned int status, void *data)
{
	struct status_file_info *sfi = data;

	sfi->count++;
	sfi->status = status;

	if (sfi->count > 1 || strcmp(sfi->expected, path) != 0) {
		giterr_set(GITERR_INVALID,
			"Ambiguous path '%s' given to git_status_file", sfi->expected);
		return -1;
	}

	return 0;
}

int git_status_file(
	unsigned int *status_flags,
	git_repository *repo,
	const char *path)
{
	int error;
	git_status_options opts;
	struct status_file_info sfi;

	assert(status_flags && repo && path);

	memset(&sfi, 0, sizeof(sfi));
	if ((sfi.expected = git__strdup(path)) == NULL)
		return -1;

	memset(&opts, 0, sizeof(opts));
	opts.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	opts.flags = GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
		GIT_STATUS_OPT_INCLUDE_UNMODIFIED;
	opts.pathspec.count = 1;
	opts.pathspec.strings = &sfi.expected;

	error = git_status_foreach_ext(repo, &opts, get_one_status, &sfi);

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
	int error;
	git_ignores ignores;

	if (git_ignore__for_path(repo, path, &ignores) < 0)
		return -1;

	error = git_ignore__lookup(&ignores, path, ignored);
	git_ignore__free(&ignores);
	return error;
}

