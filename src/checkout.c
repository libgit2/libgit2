/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "git2/checkout.h"
#include "git2/repository.h"
#include "git2/refs.h"
#include "git2/tree.h"
#include "git2/blob.h"
#include "git2/config.h"
#include "git2/diff.h"

#include "common.h"
#include "refs.h"
#include "buffer.h"
#include "repository.h"
#include "filter.h"
#include "blob.h"
#include "diff.h"
#include "pathspec.h"

typedef struct {
	git_repository *repo;
	git_diff_list *diff;
	git_checkout_opts *opts;
	git_buf *path;
	size_t workdir_len;
	bool can_symlink;
	int error;
	size_t total_steps;
	size_t completed_steps;
} checkout_diff_data;

static int buffer_to_file(
	git_buf *buffer,
	const char *path,
	mode_t dir_mode,
	int file_open_flags,
	mode_t file_mode)
{
	int fd, error;

	if ((error = git_futils_mkpath2file(path, dir_mode)) < 0)
		return error;

	if ((fd = p_open(path, file_open_flags, file_mode)) < 0) {
		giterr_set(GITERR_OS, "Could not open '%s' for writing", path);
		return fd;
	}

	if ((error = p_write(fd, git_buf_cstr(buffer), git_buf_len(buffer))) < 0) {
		giterr_set(GITERR_OS, "Could not write to '%s'", path);
		(void)p_close(fd);
	} else {
		if ((error = p_close(fd)) < 0)
			giterr_set(GITERR_OS, "Error while closing '%s'", path);
	}

	if (!error &&
		(file_mode & 0100) != 0 &&
		(error = p_chmod(path, file_mode)) < 0)
		giterr_set(GITERR_OS, "Failed to set permissions on '%s'", path);

	return error;
}

static int blob_content_to_file(
	git_blob *blob,
	const char *path,
	mode_t entry_filemode,
	git_checkout_opts *opts)
{
	int error = -1, nb_filters = 0;
	mode_t file_mode = opts->file_mode;
	bool dont_free_filtered = false;
	git_buf unfiltered = GIT_BUF_INIT, filtered = GIT_BUF_INIT;
	git_vector filters = GIT_VECTOR_INIT;

	if (opts->disable_filters ||
		(nb_filters = git_filters_load(
			&filters,
			git_object_owner((git_object *)blob),
			path,
			GIT_FILTER_TO_WORKTREE)) == 0) {

		/* Create a fake git_buf from the blob raw data... */
		filtered.ptr = blob->odb_object->raw.data;
		filtered.size = blob->odb_object->raw.len;

		/* ... and make sure it doesn't get unexpectedly freed */
		dont_free_filtered = true;
	}

	if (nb_filters < 0)
		return nb_filters;

	if (nb_filters > 0)	 {
		if ((error = git_blob__getbuf(&unfiltered, blob)) < 0)
			goto cleanup;

		if ((error = git_filters_apply(&filtered, &unfiltered, &filters)) < 0)
			goto cleanup;
	}

	/* Allow overriding of file mode */
	if (!file_mode)
		file_mode = entry_filemode;

	error = buffer_to_file(
		&filtered, path, opts->dir_mode, opts->file_open_flags, file_mode);

cleanup:
	git_filters_free(&filters);
	git_buf_free(&unfiltered);
	if (!dont_free_filtered)
		git_buf_free(&filtered);

	return error;
}

static int blob_content_to_link(
	git_blob *blob, const char *path, bool can_symlink)
{
	git_buf linktarget = GIT_BUF_INIT;
	int error;

	if ((error = git_blob__getbuf(&linktarget, blob)) < 0)
		return error;

	if (can_symlink)
		error = p_symlink(git_buf_cstr(&linktarget), path);
	else
		error = git_futils_fake_symlink(git_buf_cstr(&linktarget), path);

	git_buf_free(&linktarget);

	return error;
}

static int checkout_submodule(
	checkout_diff_data *data,
	const git_diff_file *file)
{
	/* Until submodules are supported, UPDATE_ONLY means do nothing here */
	if ((data->opts->checkout_strategy & GIT_CHECKOUT_UPDATE_ONLY) != 0)
		return 0;

	if (git_futils_mkdir(
			file->path, git_repository_workdir(data->repo),
			data->opts->dir_mode, GIT_MKDIR_PATH) < 0)
		return -1;

	/* TODO: Support checkout_strategy options.  Two circumstances:
	 * 1 - submodule already checked out, but we need to move the HEAD
	 *     to the new OID, or
	 * 2 - submodule not checked out and we should recursively check it out
	 *
	 * Checkout will not execute a pull on the submodule, but a clone
	 * command should probably be able to.  Do we need a submodule callback?
	 */

	return 0;
}

static void report_progress(
	checkout_diff_data *data,
	const char *path)
{
	if (data->opts->progress_cb)
		data->opts->progress_cb(
			path, data->completed_steps, data->total_steps,
			data->opts->progress_payload);
}

static int checkout_blob(
	checkout_diff_data *data,
	const git_diff_file *file)
{
	int error = 0;
	git_blob *blob;

	git_buf_truncate(data->path, data->workdir_len);
	if (git_buf_puts(data->path, file->path) < 0)
		return -1;

	if ((error = git_blob_lookup(&blob, data->repo, &file->oid)) < 0)
		return error;

	if (S_ISLNK(file->mode))
		error = blob_content_to_link(
			blob, git_buf_cstr(data->path), data->can_symlink);
	else
		error = blob_content_to_file(
			blob, git_buf_cstr(data->path), file->mode, data->opts);

	git_blob_free(blob);

	return error;
}

static int retrieve_symlink_caps(git_repository *repo, bool *can_symlink)
{
	git_config *cfg;
	int error;

	if (git_repository_config__weakptr(&cfg, repo) < 0)
		return -1;

	error = git_config_get_bool((int *)can_symlink, cfg, "core.symlinks");

	/* If "core.symlinks" is not found anywhere, default to true. */
	if (error == GIT_ENOTFOUND) {
		*can_symlink = true;
		error = 0;
	}

	return error;
}

static void normalize_options(
	git_checkout_opts *normalized, git_checkout_opts *proposed)
{
	assert(normalized);

	if (!proposed)
		memset(normalized, 0, sizeof(git_checkout_opts));
	else
		memmove(normalized, proposed, sizeof(git_checkout_opts));

	/* implied checkout strategies */
	if ((normalized->checkout_strategy & GIT_CHECKOUT_UPDATE_MODIFIED) != 0 ||
		(normalized->checkout_strategy & GIT_CHECKOUT_UPDATE_UNTRACKED) != 0)
		normalized->checkout_strategy |= GIT_CHECKOUT_UPDATE_UNMODIFIED;

	if ((normalized->checkout_strategy & GIT_CHECKOUT_UPDATE_UNTRACKED) != 0)
		normalized->checkout_strategy |= GIT_CHECKOUT_UPDATE_MISSING;

	/* opts->disable_filters is false by default */

	if (!normalized->dir_mode)
		normalized->dir_mode = GIT_DIR_MODE;

	if (!normalized->file_open_flags)
		normalized->file_open_flags = O_CREAT | O_TRUNC | O_WRONLY;
}

enum {
	CHECKOUT_ACTION__NONE = 0,
	CHECKOUT_ACTION__REMOVE = 1,
	CHECKOUT_ACTION__UPDATE_BLOB = 2,
	CHECKOUT_ACTION__UPDATE_SUBMODULE = 4,
	CHECKOUT_ACTION__CONFLICT = 8,
	CHECKOUT_ACTION__MAX = 8
};

static int checkout_confirm_update_blob(
	checkout_diff_data *data,
	const git_diff_delta *delta,
	int action)
{
	int error;
	unsigned int strat = data->opts->checkout_strategy;
	struct stat st;
	bool update_only = ((strat & GIT_CHECKOUT_UPDATE_ONLY) != 0);

	/* for typechange, remove the old item first */
	if (delta->status == GIT_DELTA_TYPECHANGE) {
		if (update_only)
			action = CHECKOUT_ACTION__NONE;
		else
			action |= CHECKOUT_ACTION__REMOVE;

		return action;
	}

	git_buf_truncate(data->path, data->workdir_len);
	if (git_buf_puts(data->path, delta->new_file.path) < 0)
		return -1;

	if ((error = p_lstat_posixly(git_buf_cstr(data->path), &st)) < 0) {
		if (errno == ENOENT) {
			if (update_only)
				action = CHECKOUT_ACTION__NONE;
		} else if (errno == ENOTDIR) {
			/* File exists where a parent dir needs to go - i.e. untracked
			 * typechange.  Ignore if UPDATE_ONLY, remove if allowed.
			 */
			if (update_only)
				action = CHECKOUT_ACTION__NONE;
			else if ((strat & GIT_CHECKOUT_UPDATE_UNTRACKED) != 0)
				action |= CHECKOUT_ACTION__REMOVE;
			else
				action = CHECKOUT_ACTION__CONFLICT;
		}
		/* otherwise let error happen when we attempt blob checkout later */
	}
	else if (S_ISDIR(st.st_mode)) {
		/* Directory exists where a blob needs to go - i.e. untracked
		 * typechange.  Ignore if UPDATE_ONLY, remove if allowed.
		 */
		if (update_only)
			action = CHECKOUT_ACTION__NONE;
		else if ((strat & GIT_CHECKOUT_UPDATE_UNTRACKED) != 0)
			action |= CHECKOUT_ACTION__REMOVE;
		else
			action = CHECKOUT_ACTION__CONFLICT;
	}

	return action;
}

static int checkout_action_for_delta(
	checkout_diff_data *data,
	const git_diff_delta *delta,
	const git_index_entry *head_entry)
{
	int action = CHECKOUT_ACTION__NONE;
	unsigned int strat  = data->opts->checkout_strategy;

	switch (delta->status) {
	case GIT_DELTA_UNMODIFIED:
		if (!head_entry) {
			/* file independently created in wd, even though not in HEAD */
			if ((strat & GIT_CHECKOUT_UPDATE_MISSING) == 0)
				action = CHECKOUT_ACTION__CONFLICT;
		}
		else if (!git_oid_equal(&head_entry->oid, &delta->old_file.oid)) {
			/* working directory was independently updated to match index */
			if ((strat & GIT_CHECKOUT_UPDATE_MODIFIED) == 0)
				action = CHECKOUT_ACTION__CONFLICT;
		}
		break;

	case GIT_DELTA_ADDED:
		/* Impossible.  New files should be UNTRACKED or TYPECHANGE */
		action = CHECKOUT_ACTION__CONFLICT;
		break;

	case GIT_DELTA_DELETED:
		if (head_entry && /* working dir missing, but exists in HEAD */
			(strat & GIT_CHECKOUT_UPDATE_MISSING) == 0)
			action = CHECKOUT_ACTION__CONFLICT;
		else
			action = CHECKOUT_ACTION__UPDATE_BLOB;
		break;

	case GIT_DELTA_MODIFIED:
	case GIT_DELTA_TYPECHANGE:
		if (!head_entry) {
			/* working dir was independently updated & does not match index */
			if ((strat & GIT_CHECKOUT_UPDATE_UNTRACKED) == 0)
				action = CHECKOUT_ACTION__CONFLICT;
			else
				action = CHECKOUT_ACTION__UPDATE_BLOB;
		}
		else if (git_oid_equal(&head_entry->oid, &delta->new_file.oid))
			action = CHECKOUT_ACTION__UPDATE_BLOB;
		else if ((strat & GIT_CHECKOUT_UPDATE_MODIFIED) == 0)
			action = CHECKOUT_ACTION__CONFLICT;
		else
			action = CHECKOUT_ACTION__UPDATE_BLOB;
		break;

	case GIT_DELTA_UNTRACKED:
		if (!head_entry) {
			if ((strat & GIT_CHECKOUT_REMOVE_UNTRACKED) != 0)
				action = CHECKOUT_ACTION__REMOVE;
		}
		else if ((strat & GIT_CHECKOUT_UPDATE_MODIFIED) != 0) {
			action = CHECKOUT_ACTION__REMOVE;
		} else if ((strat & GIT_CHECKOUT_UPDATE_UNMODIFIED) != 0) {
			git_oid wd_oid;

			/* if HEAD matches workdir, then remove, else conflict */

			if (git_oid_iszero(&delta->new_file.oid) &&
				git_diff__oid_for_file(
					data->repo, delta->new_file.path, delta->new_file.mode,
					delta->new_file.size, &wd_oid) < 0)
				action = -1;
			else if (git_oid_equal(&head_entry->oid, &wd_oid))
				action = CHECKOUT_ACTION__REMOVE;
			else
				action = CHECKOUT_ACTION__CONFLICT;
		} else {
			/* present in HEAD and workdir, but absent in index */
			action = CHECKOUT_ACTION__CONFLICT;
		}
		break;

	case GIT_DELTA_IGNORED:
	default:
		/* just skip these files */
		break;
	}

	if (action > 0 && (action & CHECKOUT_ACTION__UPDATE_BLOB) != 0) {
		if (S_ISGITLINK(delta->old_file.mode))
			action = (action & ~CHECKOUT_ACTION__UPDATE_BLOB) |
				CHECKOUT_ACTION__UPDATE_SUBMODULE;

		action = checkout_confirm_update_blob(data, delta, action);
	}

	if (action == CHECKOUT_ACTION__CONFLICT &&
		data->opts->conflict_cb != NULL &&
		data->opts->conflict_cb(
			delta->old_file.path, &delta->old_file.oid,
			delta->old_file.mode, delta->new_file.mode,
			data->opts->conflict_payload) != 0)
	{
		giterr_clear();
		action = GIT_EUSER;
	}

	if (action > 0 && (strat & GIT_CHECKOUT_UPDATE_ONLY) != 0)
		action = (action & ~CHECKOUT_ACTION__REMOVE);

	return action;
}

static int checkout_get_actions(
	uint32_t **actions_ptr,
	size_t **counts_ptr,
	checkout_diff_data *data)
{
	int error;
	git_diff_list *diff = data->diff;
	git_diff_delta *delta;
	size_t i, *counts = NULL;
	uint32_t *actions = NULL;
	git_tree *head = NULL;
	git_iterator *hiter = NULL;
	char *pfx = git_pathspec_prefix(&data->opts->paths);
	const git_index_entry *he;

	/* if there is no HEAD, that's okay - we'll make an empty iterator */
	(void)git_repository_head_tree(&head, data->repo);

	if ((error = git_iterator_for_tree_range(
			 &hiter, data->repo, head, pfx, pfx)) < 0)
		goto fail;

	if ((diff->opts.flags & GIT_DIFF_DELTAS_ARE_ICASE) != 0 &&
		!hiter->ignore_case &&
		(error = git_iterator_spoolandsort(
			&hiter, hiter, diff->entrycomp, true)) < 0)
		goto fail;

	if ((error = git_iterator_current(hiter, &he)) < 0)
		goto fail;

	git__free(pfx);
	pfx = NULL;

	*counts_ptr = counts = git__calloc(CHECKOUT_ACTION__MAX+1, sizeof(size_t));
	*actions_ptr = actions = git__calloc(diff->deltas.length, sizeof(uint32_t));
	if (!counts || !actions) {
		error = -1;
		goto fail;
	}

	git_vector_foreach(&diff->deltas, i, delta) {
		int cmp = -1, act;

		/* try to track HEAD entries parallel to deltas */
		while (he) {
			cmp = S_ISDIR(delta->new_file.mode) ?
				diff->pfxcomp(he->path, delta->new_file.path) :
				diff->strcomp(he->path, delta->old_file.path);
			if (cmp >= 0)
				break;
			if (git_iterator_advance(hiter, &he) < 0)
				he = NULL;
		}

		act = checkout_action_for_delta(data, delta, !cmp ? he : NULL);

		if (act < 0) {
			error = act;
			goto fail;
		}

		if (!cmp && git_iterator_advance(hiter, &he) < 0)
			he = NULL;

		actions[i] = act;

		if (act & CHECKOUT_ACTION__REMOVE)
			counts[CHECKOUT_ACTION__REMOVE]++;
		if (act & CHECKOUT_ACTION__UPDATE_BLOB)
			counts[CHECKOUT_ACTION__UPDATE_BLOB]++;
		if (act & CHECKOUT_ACTION__UPDATE_SUBMODULE)
			counts[CHECKOUT_ACTION__UPDATE_SUBMODULE]++;
		if (act & CHECKOUT_ACTION__CONFLICT)
			counts[CHECKOUT_ACTION__CONFLICT]++;
	}

	if (counts[CHECKOUT_ACTION__CONFLICT] > 0 &&
		(data->opts->checkout_strategy & GIT_CHECKOUT_ALLOW_CONFLICTS) == 0)
	{
		giterr_set(GITERR_CHECKOUT, "%d conflicts prevent checkout",
				   (int)counts[CHECKOUT_ACTION__CONFLICT]);
		goto fail;
	}

	git_iterator_free(hiter);
	git_tree_free(head);

	return 0;

fail:
	*counts_ptr = NULL;
	git__free(counts);
	*actions_ptr = NULL;
	git__free(actions);

	git_iterator_free(hiter);
	git_tree_free(head);
	git__free(pfx);

	return -1;
}

static int checkout_remove_the_old(
	git_diff_list *diff,
	unsigned int *actions,
	checkout_diff_data *data)
{
	git_diff_delta *delta;
	size_t i;

	git_buf_truncate(data->path, data->workdir_len);

	git_vector_foreach(&diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__REMOVE) {
			int error = git_futils_rmdir_r(
				delta->new_file.path,
				git_buf_cstr(data->path), /* here set to work dir root */
				GIT_RMDIR_REMOVE_FILES | GIT_RMDIR_EMPTY_PARENTS |
				GIT_RMDIR_REMOVE_BLOCKERS);
			if (error < 0)
				return error;

			data->completed_steps++;
			report_progress(data, delta->new_file.path);
		}
	}

	return 0;
}

static int checkout_create_the_new(
	git_diff_list *diff,
	unsigned int *actions,
	checkout_diff_data *data)
{
	git_diff_delta *delta;
	size_t i;

	git_vector_foreach(&diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__UPDATE_BLOB) {
			int error = checkout_blob(data, &delta->old_file);
			if (error < 0)
				return error;

			data->completed_steps++;
			report_progress(data, delta->old_file.path);
		}
	}

	return 0;
}

static int checkout_create_submodules(
	git_diff_list *diff,
	unsigned int *actions,
	checkout_diff_data *data)
{
	git_diff_delta *delta;
	size_t i;

	git_vector_foreach(&diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__UPDATE_SUBMODULE) {
			int error = checkout_submodule(data, &delta->old_file);
			if (error < 0)
				return error;

			data->completed_steps++;
			report_progress(data, delta->old_file.path);
		}
	}

	return 0;
}

int git_checkout_index(
	git_repository *repo,
	git_index *index,
	git_checkout_opts *opts)
{
	git_diff_list *diff = NULL;
	git_diff_options diff_opts = {0};
	git_checkout_opts checkout_opts;
	checkout_diff_data data;
	git_buf workdir = GIT_BUF_INIT;
	uint32_t *actions = NULL;
	size_t *counts = NULL;
	int error;

	assert(repo);

	if ((error = git_repository__ensure_not_bare(repo, "checkout")) < 0)
		return error;

	diff_opts.flags =
		GIT_DIFF_INCLUDE_UNMODIFIED | GIT_DIFF_INCLUDE_UNTRACKED |
		GIT_DIFF_INCLUDE_TYPECHANGE | GIT_DIFF_SKIP_BINARY_CHECK;

	if (opts && opts->paths.count > 0)
		diff_opts.pathspec = opts->paths;

	if ((error = git_diff_workdir_to_index(&diff, repo, index, &diff_opts)) < 0)
		goto cleanup;

	if ((error = git_buf_puts(&workdir, git_repository_workdir(repo))) < 0)
		goto cleanup;

	normalize_options(&checkout_opts, opts);

	/* Checkout is best performed with up to four passes through the diff.
	 *
	 * 0. Figure out what actions should be taken and record for later.
	 * 1. Next do removes, because we iterate in alphabetical order, thus
	 *    a new untracked directory will end up sorted *after* a blob that
	 *    should be checked out with the same name.
	 * 2. Then checkout all blobs.
	 * 3. Then checkout all submodules in case a new .gitmodules blob was
	 *    checked out during pass #2.
	 */

	memset(&data, 0, sizeof(data));
	data.path = &workdir;
	data.workdir_len = git_buf_len(&workdir);
	data.repo = repo;
	data.diff = diff;
	data.opts = &checkout_opts;

	if ((error = checkout_get_actions(&actions, &counts, &data)) < 0)
		goto cleanup;

	data.total_steps = counts[CHECKOUT_ACTION__REMOVE] +
		counts[CHECKOUT_ACTION__UPDATE_BLOB] +
		counts[CHECKOUT_ACTION__UPDATE_SUBMODULE];

	if ((error = retrieve_symlink_caps(repo, &data.can_symlink)) < 0)
		goto cleanup;

	report_progress(&data, NULL); /* establish 0 baseline */

	if (counts[CHECKOUT_ACTION__REMOVE] > 0 &&
		(error = checkout_remove_the_old(diff, actions, &data)) < 0)
		goto cleanup;

	if (counts[CHECKOUT_ACTION__UPDATE_BLOB] > 0 &&
		(error = checkout_create_the_new(diff, actions, &data)) < 0)
		goto cleanup;

	if (counts[CHECKOUT_ACTION__UPDATE_SUBMODULE] > 0 &&
		(error = checkout_create_submodules(diff, actions, &data)) < 0)
		goto cleanup;

	assert(data.completed_steps == data.total_steps);

cleanup:
	if (error == GIT_EUSER)
		giterr_clear();

	git__free(actions);
	git__free(counts);
	git_diff_list_free(diff);
	git_buf_free(&workdir);

	return error;
}

int git_checkout_tree(
	git_repository *repo,
	const git_object *treeish,
	git_checkout_opts *opts)
{
	int error = 0;
	git_index *index = NULL;
	git_tree *tree = NULL;

	assert(repo && treeish);

	if (git_object_peel((git_object **)&tree, treeish, GIT_OBJ_TREE) < 0) {
		giterr_set(
			GITERR_CHECKOUT, "Provided object cannot be peeled to a tree");
		return -1;
	}

	/* TODO: create a temp index, load tree there and check it out */

	/* load paths in tree that match pathspec into index */
	if (!(error = git_repository_index(&index, repo)) &&
		!(error = git_index_read_tree_match(
			index, tree, opts ? &opts->paths : NULL)) &&
		!(error = git_index_write(index)))
		error = git_checkout_index(repo, NULL, opts);

	git_index_free(index);
	git_tree_free(tree);

	return error;
}

int git_checkout_head(
	git_repository *repo,
	git_checkout_opts *opts)
{
	int error;
	git_reference *head = NULL;
	git_object *tree = NULL;

	assert(repo);

	if (!(error = git_repository_head(&head, repo)) &&
		!(error = git_reference_peel(&tree, head, GIT_OBJ_TREE)))
		error = git_checkout_tree(repo, tree, opts);

	git_reference_free(head);
	git_object_free(tree);

	return error;
}
