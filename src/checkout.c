/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "checkout.h"

#include "git2/repository.h"
#include "git2/refs.h"
#include "git2/tree.h"
#include "git2/blob.h"
#include "git2/config.h"
#include "git2/diff.h"
#include "git2/submodule.h"

#include "refs.h"
#include "repository.h"
#include "filter.h"
#include "blob.h"
#include "diff.h"
#include "pathspec.h"

/* See docs/checkout-internals.md for more information */

enum {
	CHECKOUT_ACTION__NONE = 0,
	CHECKOUT_ACTION__REMOVE = 1,
	CHECKOUT_ACTION__UPDATE_BLOB = 2,
	CHECKOUT_ACTION__UPDATE_SUBMODULE = 4,
	CHECKOUT_ACTION__CONFLICT = 8,
	CHECKOUT_ACTION__MAX = 8,
	CHECKOUT_ACTION__DEFER_REMOVE = 16,
	CHECKOUT_ACTION__REMOVE_AND_UPDATE =
		(CHECKOUT_ACTION__UPDATE_BLOB | CHECKOUT_ACTION__REMOVE),
};

typedef struct {
	git_repository *repo;
	git_diff_list *diff;
	git_checkout_opts opts;
	bool opts_free_baseline;
	char *pfx;
	git_index *index;
	git_pool pool;
	git_vector removes;
	git_buf path;
	size_t workdir_len;
	unsigned int strategy;
	int can_symlink;
	bool reload_submodules;
	size_t total_steps;
	size_t completed_steps;
} checkout_data;

static int checkout_notify(
	checkout_data *data,
	git_checkout_notify_t why,
	const git_diff_delta *delta,
	const git_index_entry *wditem)
{
	git_diff_file wdfile;
	const git_diff_file *baseline = NULL, *target = NULL, *workdir = NULL;
	const char *path = NULL;

	if (!data->opts.notify_cb)
		return 0;

	if ((why & data->opts.notify_flags) == 0)
		return 0;

	if (wditem) {
		memset(&wdfile, 0, sizeof(wdfile));

		git_oid_cpy(&wdfile.oid, &wditem->oid);
		wdfile.path = wditem->path;
		wdfile.size = wditem->file_size;
		wdfile.flags = GIT_DIFF_FLAG_VALID_OID;
		wdfile.mode = wditem->mode;

		workdir = &wdfile;

		path = wditem->path;
	}

	if (delta) {
		switch (delta->status) {
		case GIT_DELTA_UNMODIFIED:
		case GIT_DELTA_MODIFIED:
		case GIT_DELTA_TYPECHANGE:
		default:
			baseline = &delta->old_file;
			target = &delta->new_file;
			break;
		case GIT_DELTA_ADDED:
		case GIT_DELTA_IGNORED:
		case GIT_DELTA_UNTRACKED:
			target = &delta->new_file;
			break;
		case GIT_DELTA_DELETED:
			baseline = &delta->old_file;
			break;
		}

		path = delta->old_file.path;
	}

	return data->opts.notify_cb(
		why, path, baseline, target, workdir, data->opts.notify_payload);
}

static bool checkout_is_workdir_modified(
	checkout_data *data,
	const git_diff_file *baseitem,
	const git_index_entry *wditem)
{
	git_oid oid;

	/* handle "modified" submodule */
	if (wditem->mode == GIT_FILEMODE_COMMIT) {
		git_submodule *sm;
		unsigned int sm_status = 0;
		const git_oid *sm_oid = NULL;

		if (git_submodule_lookup(&sm, data->repo, wditem->path) < 0 ||
			git_submodule_status(&sm_status, sm) < 0)
			return true;

		if (GIT_SUBMODULE_STATUS_IS_WD_DIRTY(sm_status))
			return true;

		sm_oid = git_submodule_wd_id(sm);
		if (!sm_oid)
			return false;

		return (git_oid_cmp(&baseitem->oid, sm_oid) != 0);
	}

	/* depending on where base is coming from, we may or may not know
	 * the actual size of the data, so we can't rely on this shortcut.
	 */
	if (baseitem->size && wditem->file_size != baseitem->size)
		return true;

	if (git_diff__oid_for_file(
			data->repo, wditem->path, wditem->mode,
			wditem->file_size, &oid) < 0)
		return false;

	return (git_oid_cmp(&baseitem->oid, &oid) != 0);
}

#define CHECKOUT_ACTION_IF(FLAG,YES,NO) \
	((data->strategy & GIT_CHECKOUT_##FLAG) ? CHECKOUT_ACTION__##YES : CHECKOUT_ACTION__##NO)

static int checkout_action_common(
	checkout_data *data,
	int action,
	const git_diff_delta *delta,
	const git_index_entry *wd)
{
	git_checkout_notify_t notify = GIT_CHECKOUT_NOTIFY_NONE;

	if (action <= 0)
		return action;

	if ((data->strategy & GIT_CHECKOUT_UPDATE_ONLY) != 0)
		action = (action & ~CHECKOUT_ACTION__REMOVE);

	if ((action & CHECKOUT_ACTION__UPDATE_BLOB) != 0) {
		if (S_ISGITLINK(delta->new_file.mode))
			action = (action & ~CHECKOUT_ACTION__UPDATE_BLOB) |
				CHECKOUT_ACTION__UPDATE_SUBMODULE;

		notify = GIT_CHECKOUT_NOTIFY_UPDATED;
	}

	if ((action & CHECKOUT_ACTION__CONFLICT) != 0)
		notify = GIT_CHECKOUT_NOTIFY_CONFLICT;

	if (notify != GIT_CHECKOUT_NOTIFY_NONE &&
		checkout_notify(data, notify, delta, wd) != 0)
		return GIT_EUSER;

	return action;
}

static int checkout_action_no_wd(
	checkout_data *data,
	const git_diff_delta *delta)
{
	int action = CHECKOUT_ACTION__NONE;

	switch (delta->status) {
	case GIT_DELTA_UNMODIFIED: /* case 12 */
		if (checkout_notify(data, GIT_CHECKOUT_NOTIFY_DIRTY, delta, NULL))
			return GIT_EUSER;
		action = CHECKOUT_ACTION_IF(SAFE_CREATE, UPDATE_BLOB, NONE);
		break;
	case GIT_DELTA_ADDED:    /* case 2 or 28 (and 5 but not really) */
	case GIT_DELTA_MODIFIED: /* case 13 (and 35 but not really) */
		action = CHECKOUT_ACTION_IF(SAFE, UPDATE_BLOB, NONE);
		break;
	case GIT_DELTA_TYPECHANGE: /* case 21 (B->T) and 28 (T->B)*/
		if (delta->new_file.mode == GIT_FILEMODE_TREE)
			action = CHECKOUT_ACTION_IF(SAFE, UPDATE_BLOB, NONE);
		break;
	case GIT_DELTA_DELETED: /* case 8 or 25 */
	default: /* impossible */
		break;
	}

	return checkout_action_common(data, action, delta, NULL);
}

static int checkout_action_wd_only(
	checkout_data *data,
	git_iterator *workdir,
	const git_index_entry *wd,
	git_vector *pathspec)
{
	bool remove = false;
	git_checkout_notify_t notify = GIT_CHECKOUT_NOTIFY_NONE;

	if (!git_pathspec_match_path(
			pathspec, wd->path,
			(data->strategy & GIT_CHECKOUT_DISABLE_PATHSPEC_MATCH) != 0,
			git_iterator_ignore_case(workdir), NULL))
		return 0;

	/* check if item is tracked in the index but not in the checkout diff */
	if (data->index != NULL) {
		if (wd->mode != GIT_FILEMODE_TREE) {
			if (git_index_get_bypath(data->index, wd->path, 0) != NULL) {
				notify = GIT_CHECKOUT_NOTIFY_DIRTY;
				remove = ((data->strategy & GIT_CHECKOUT_FORCE) != 0);
			}
		} else {
			/* for tree entries, we have to see if there are any index
			 * entries that are contained inside that tree
			 */
			size_t pos = git_index__prefix_position(data->index, wd->path);
			const git_index_entry *e = git_index_get_byindex(data->index, pos);

			if (e != NULL && data->diff->pfxcomp(e->path, wd->path) == 0) {
				notify = GIT_CHECKOUT_NOTIFY_DIRTY;
				remove = ((data->strategy & GIT_CHECKOUT_FORCE) != 0);
			}
		}
	}

	if (notify != GIT_CHECKOUT_NOTIFY_NONE)
		/* found in index */;
	else if (git_iterator_current_is_ignored(workdir)) {
		notify = GIT_CHECKOUT_NOTIFY_IGNORED;
		remove = ((data->strategy & GIT_CHECKOUT_REMOVE_IGNORED) != 0);
	}
	else {
		notify = GIT_CHECKOUT_NOTIFY_UNTRACKED;
		remove = ((data->strategy & GIT_CHECKOUT_REMOVE_UNTRACKED) != 0);
	}

	if (checkout_notify(data, notify, NULL, wd))
		return GIT_EUSER;

	if (remove) {
		char *path = git_pool_strdup(&data->pool, wd->path);
		GITERR_CHECK_ALLOC(path);

		if (git_vector_insert(&data->removes, path) < 0)
			return -1;
	}

	return 0;
}

static bool submodule_is_config_only(
	checkout_data *data,
	const char *path)
{
	git_submodule *sm = NULL;
	unsigned int sm_loc = 0;

	if (git_submodule_lookup(&sm, data->repo, path) < 0 ||
		git_submodule_location(&sm_loc, sm) < 0 ||
		sm_loc == GIT_SUBMODULE_STATUS_IN_CONFIG)
		return true;

	return false;
}

static int checkout_action_with_wd(
	checkout_data *data,
	const git_diff_delta *delta,
	const git_index_entry *wd)
{
	int action = CHECKOUT_ACTION__NONE;

	switch (delta->status) {
	case GIT_DELTA_UNMODIFIED: /* case 14/15 or 33 */
		if (checkout_is_workdir_modified(data, &delta->old_file, wd)) {
			if (checkout_notify(
					data, GIT_CHECKOUT_NOTIFY_DIRTY, delta, wd))
				return GIT_EUSER;
			action = CHECKOUT_ACTION_IF(FORCE, UPDATE_BLOB, NONE);
		}
		break;
	case GIT_DELTA_ADDED: /* case 3, 4 or 6 */
		action = CHECKOUT_ACTION_IF(FORCE, UPDATE_BLOB, CONFLICT);
		break;
	case GIT_DELTA_DELETED: /* case 9 or 10 (or 26 but not really) */
		if (checkout_is_workdir_modified(data, &delta->old_file, wd))
			action = CHECKOUT_ACTION_IF(FORCE, REMOVE, CONFLICT);
		else
			action = CHECKOUT_ACTION_IF(SAFE, REMOVE, NONE);
		break;
	case GIT_DELTA_MODIFIED: /* case 16, 17, 18 (or 36 but not really) */
		if (checkout_is_workdir_modified(data, &delta->old_file, wd))
			action = CHECKOUT_ACTION_IF(FORCE, UPDATE_BLOB, CONFLICT);
		else
			action = CHECKOUT_ACTION_IF(SAFE, UPDATE_BLOB, NONE);
		break;
	case GIT_DELTA_TYPECHANGE: /* case 22, 23, 29, 30 */
		if (delta->old_file.mode == GIT_FILEMODE_TREE) {
			if (wd->mode == GIT_FILEMODE_TREE)
				/* either deleting items in old tree will delete the wd dir,
				 * or we'll get a conflict when we attempt blob update...
				 */
				action = CHECKOUT_ACTION_IF(SAFE, UPDATE_BLOB, NONE);
			else if (wd->mode == GIT_FILEMODE_COMMIT) {
				/* workdir is possibly a "phantom" submodule - treat as a
				 * tree if the only submodule info came from the config
				 */
				if (submodule_is_config_only(data, wd->path))
					action = CHECKOUT_ACTION_IF(SAFE, UPDATE_BLOB, NONE);
				else
					action = CHECKOUT_ACTION_IF(FORCE, REMOVE_AND_UPDATE, CONFLICT);
			} else
				action = CHECKOUT_ACTION_IF(FORCE, REMOVE, CONFLICT);
		}
		else if (checkout_is_workdir_modified(data, &delta->old_file, wd))
			action = CHECKOUT_ACTION_IF(FORCE, REMOVE_AND_UPDATE, CONFLICT);
		else
			action = CHECKOUT_ACTION_IF(SAFE, REMOVE_AND_UPDATE, NONE);

		/* don't update if the typechange is to a tree */
		if (delta->new_file.mode == GIT_FILEMODE_TREE)
			action = (action & ~CHECKOUT_ACTION__UPDATE_BLOB);
		break;
	default: /* impossible */
		break;
	}

	return checkout_action_common(data, action, delta, wd);
}

static int checkout_action_with_wd_blocker(
	checkout_data *data,
	const git_diff_delta *delta,
	const git_index_entry *wd)
{
	int action = CHECKOUT_ACTION__NONE;

	switch (delta->status) {
	case GIT_DELTA_UNMODIFIED:
		/* should show delta as dirty / deleted */
		if (checkout_notify(data, GIT_CHECKOUT_NOTIFY_DIRTY, delta, wd))
			return GIT_EUSER;
		action = CHECKOUT_ACTION_IF(FORCE, REMOVE_AND_UPDATE, NONE);
		break;
	case GIT_DELTA_ADDED:
	case GIT_DELTA_MODIFIED:
		action = CHECKOUT_ACTION_IF(FORCE, REMOVE_AND_UPDATE, CONFLICT);
		break;
	case GIT_DELTA_DELETED:
		action = CHECKOUT_ACTION_IF(FORCE, REMOVE, CONFLICT);
		break;
	case GIT_DELTA_TYPECHANGE:
		/* not 100% certain about this... */
		action = CHECKOUT_ACTION_IF(FORCE, REMOVE_AND_UPDATE, CONFLICT);
		break;
	default: /* impossible */
		break;
	}

	return checkout_action_common(data, action, delta, wd);
}

static int checkout_action_with_wd_dir(
	checkout_data *data,
	const git_diff_delta *delta,
	const git_index_entry *wd)
{
	int action = CHECKOUT_ACTION__NONE;

	switch (delta->status) {
	case GIT_DELTA_UNMODIFIED: /* case 19 or 24 (or 34 but not really) */
		if (checkout_notify(data, GIT_CHECKOUT_NOTIFY_DIRTY, delta, NULL) ||
			checkout_notify(
				data, GIT_CHECKOUT_NOTIFY_UNTRACKED, NULL, wd))
			return GIT_EUSER;
		break;
	case GIT_DELTA_ADDED:/* case 4 (and 7 for dir) */
	case GIT_DELTA_MODIFIED: /* case 20 (or 37 but not really) */
		if (delta->old_file.mode == GIT_FILEMODE_COMMIT)
			/* expected submodule (and maybe found one) */;
		else if (delta->new_file.mode != GIT_FILEMODE_TREE)
			action = CHECKOUT_ACTION_IF(FORCE, REMOVE_AND_UPDATE, CONFLICT);
		break;
	case GIT_DELTA_DELETED: /* case 11 (and 27 for dir) */
		if (delta->old_file.mode != GIT_FILEMODE_TREE &&
			checkout_notify(
				data, GIT_CHECKOUT_NOTIFY_UNTRACKED, NULL, wd))
			return GIT_EUSER;
		break;
	case GIT_DELTA_TYPECHANGE: /* case 24 or 31 */
		if (delta->old_file.mode == GIT_FILEMODE_TREE) {
			/* For typechange from dir, remove dir and add blob, but it is
			 * not safe to remove dir if it contains modified files.
			 * However, safely removing child files will remove the parent
			 * directory if is it left empty, so we can defer removing the
			 * dir and it will succeed if no children are left.
			 */
			action = CHECKOUT_ACTION_IF(SAFE, UPDATE_BLOB, NONE);
			if (action != CHECKOUT_ACTION__NONE)
				action |= CHECKOUT_ACTION__DEFER_REMOVE;
		}
		else if (delta->new_file.mode != GIT_FILEMODE_TREE)
			/* For typechange to dir, dir is already created so no action */
			action = CHECKOUT_ACTION_IF(FORCE, REMOVE_AND_UPDATE, CONFLICT);
		break;
	default: /* impossible */
		break;
	}

	return checkout_action_common(data, action, delta, wd);
}

static int checkout_action(
	checkout_data *data,
	git_diff_delta *delta,
	git_iterator *workdir,
	const git_index_entry **wditem_ptr,
	git_vector *pathspec)
{
	const git_index_entry *wd = *wditem_ptr;
	int cmp = -1, act;
	int (*strcomp)(const char *, const char *) = data->diff->strcomp;
	int (*pfxcomp)(const char *str, const char *pfx) = data->diff->pfxcomp;

	/* move workdir iterator to follow along with deltas */

	while (1) {
		if (!wd)
			return checkout_action_no_wd(data, delta);

		cmp = strcomp(wd->path, delta->old_file.path);

		/* 1. wd before delta ("a/a" before "a/b")
		 * 2. wd prefixes delta & should expand ("a/" before "a/b")
		 * 3. wd prefixes delta & cannot expand ("a/b" before "a/b/c")
		 * 4. wd equals delta ("a/b" and "a/b")
		 * 5. wd after delta & delta prefixes wd ("a/b/c" after "a/b/" or "a/b")
		 * 6. wd after delta ("a/c" after "a/b")
		 */

		if (cmp < 0) {
			cmp = pfxcomp(delta->old_file.path, wd->path);

			if (cmp == 0) {
				if (wd->mode == GIT_FILEMODE_TREE) {
					/* case 2 - entry prefixed by workdir tree */
					if (git_iterator_advance_into(&wd, workdir) < 0)
						goto fail;

					*wditem_ptr = wd;
					continue;
				}

				/* case 3 maybe - wd contains non-dir where dir expected */
				if (delta->old_file.path[strlen(wd->path)] == '/') {
					act = checkout_action_with_wd_blocker(data, delta, wd);
					*wditem_ptr =
						git_iterator_advance(&wd, workdir) ? NULL : wd;
					return act;
				}
			}

			/* case 1 - handle wd item (if it matches pathspec) */
			if (checkout_action_wd_only(data, workdir, wd, pathspec) < 0 ||
				git_iterator_advance(&wd, workdir) < 0)
				goto fail;

			*wditem_ptr = wd;
			continue;
		}

		if (cmp == 0) {
			/* case 4 */
			act = checkout_action_with_wd(data, delta, wd);
			*wditem_ptr = git_iterator_advance(&wd, workdir) ? NULL : wd;
			return act;
		}

		cmp = pfxcomp(wd->path, delta->old_file.path);

		if (cmp == 0) { /* case 5 */
			if (wd->path[strlen(delta->old_file.path)] != '/')
				return checkout_action_no_wd(data, delta);

			if (delta->status == GIT_DELTA_TYPECHANGE) {
				if (delta->old_file.mode == GIT_FILEMODE_TREE) {
					act = checkout_action_with_wd(data, delta, wd);
					if (git_iterator_advance_into(&wd, workdir) < 0)
						wd = NULL;
					*wditem_ptr = wd;
					return act;
				}

				if (delta->new_file.mode == GIT_FILEMODE_TREE ||
					delta->new_file.mode == GIT_FILEMODE_COMMIT ||
					delta->old_file.mode == GIT_FILEMODE_COMMIT)
				{
					act = checkout_action_with_wd(data, delta, wd);
					if (git_iterator_advance(&wd, workdir) < 0)
						wd = NULL;
					*wditem_ptr = wd;
					return act;
				}
			}

			return checkout_action_with_wd_dir(data, delta, wd);
		}

		/* case 6 - wd is after delta */
		return checkout_action_no_wd(data, delta);
	}

fail:
	*wditem_ptr = NULL;
	return -1;
}

static int checkout_remaining_wd_items(
	checkout_data *data,
	git_iterator *workdir,
	const git_index_entry *wd,
	git_vector *spec)
{
	int error = 0;

	while (wd && !error) {
		if (!(error = checkout_action_wd_only(data, workdir, wd, spec)))
			error = git_iterator_advance(&wd, workdir);
	}

	return error;
}

static int checkout_get_actions(
	uint32_t **actions_ptr,
	size_t **counts_ptr,
	checkout_data *data,
	git_iterator *workdir)
{
	int error = 0;
	const git_index_entry *wditem;
	git_vector pathspec = GIT_VECTOR_INIT, *deltas;
	git_pool pathpool = GIT_POOL_INIT_STRINGPOOL;
	git_diff_delta *delta;
	size_t i, *counts = NULL;
	uint32_t *actions = NULL;

	if (data->opts.paths.count > 0 &&
		git_pathspec_init(&pathspec, &data->opts.paths, &pathpool) < 0)
		return -1;

	if ((error = git_iterator_current(&wditem, workdir)) < 0)
		goto fail;

	deltas = &data->diff->deltas;

	*counts_ptr = counts = git__calloc(CHECKOUT_ACTION__MAX+1, sizeof(size_t));
	*actions_ptr = actions = git__calloc(
		deltas->length ? deltas->length : 1, sizeof(uint32_t));
	if (!counts || !actions) {
		error = -1;
		goto fail;
	}

	git_vector_foreach(deltas, i, delta) {
		int act = checkout_action(data, delta, workdir, &wditem, &pathspec);

		if (act < 0) {
			error = act;
			goto fail;
		}

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

	error = checkout_remaining_wd_items(data, workdir, wditem, &pathspec);
	if (error < 0)
		goto fail;

	counts[CHECKOUT_ACTION__REMOVE] += data->removes.length;

	if (counts[CHECKOUT_ACTION__CONFLICT] > 0 &&
		(data->strategy & GIT_CHECKOUT_ALLOW_CONFLICTS) == 0)
	{
		giterr_set(GITERR_CHECKOUT, "%d conflicts prevent checkout",
			(int)counts[CHECKOUT_ACTION__CONFLICT]);
		error = GIT_EMERGECONFLICT;
		goto fail;
	}

	git_pathspec_free(&pathspec);
	git_pool_clear(&pathpool);

	return 0;

fail:
	*counts_ptr = NULL;
	git__free(counts);
	*actions_ptr = NULL;
	git__free(actions);

	git_pathspec_free(&pathspec);
	git_pool_clear(&pathpool);

	return error;
}

static int buffer_to_file(
	struct stat *st,
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
		if ((error = p_fstat(fd, st)) < 0)
			giterr_set(GITERR_OS, "Error while statting '%s'", path);

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
	struct stat *st,
	git_blob *blob,
	const char *path,
	mode_t entry_filemode,
	git_checkout_opts *opts)
{
	int error = -1, nb_filters = 0;
	mode_t file_mode = opts->file_mode;
	bool dont_free_filtered;
	git_buf unfiltered = GIT_BUF_INIT, filtered = GIT_BUF_INIT;
	git_vector filters = GIT_VECTOR_INIT;

	/* Create a fake git_buf from the blob raw data... */
	filtered.ptr = blob->odb_object->raw.data;
	filtered.size = blob->odb_object->raw.len;
	/* ... and make sure it doesn't get unexpectedly freed */
	dont_free_filtered = true;

	if (!opts->disable_filters &&
		!git_buf_text_is_binary(&filtered) &&
		(nb_filters = git_filters_load(
			&filters,
			git_object_owner((git_object *)blob),
			path,
			GIT_FILTER_TO_WORKTREE)) > 0)
	{
		/* reset 'filtered' so it can be a filter target */
		git_buf_init(&filtered, 0);
		dont_free_filtered = false;
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
		st, &filtered, path, opts->dir_mode, opts->file_open_flags, file_mode);

	if (!error)
		st->st_mode = entry_filemode;

cleanup:
	git_filters_free(&filters);
	git_buf_free(&unfiltered);
	if (!dont_free_filtered)
		git_buf_free(&filtered);

	return error;
}

static int blob_content_to_link(
	struct stat *st, git_blob *blob, const char *path, int can_symlink)
{
	git_buf linktarget = GIT_BUF_INIT;
	int error;

	if ((error = git_blob__getbuf(&linktarget, blob)) < 0)
		return error;

	if (can_symlink) {
		if ((error = p_symlink(git_buf_cstr(&linktarget), path)) < 0)
			giterr_set(GITERR_CHECKOUT, "Could not create symlink %s\n", path);
	} else {
		error = git_futils_fake_symlink(git_buf_cstr(&linktarget), path);
	}

	if (!error) {
		if ((error = p_lstat(path, st)) < 0)
			giterr_set(GITERR_CHECKOUT, "Could not stat symlink %s", path);

		st->st_mode = GIT_FILEMODE_LINK;
	}

	git_buf_free(&linktarget);

	return error;
}

static int checkout_update_index(
	checkout_data *data,
	const git_diff_file *file,
	struct stat *st)
{
	git_index_entry entry;

	if (!data->index)
		return 0;

	memset(&entry, 0, sizeof(entry));
	entry.path = (char *)file->path; /* cast to prevent warning */
	git_index_entry__init_from_stat(&entry, st);
	git_oid_cpy(&entry.oid, &file->oid);

	return git_index_add(data->index, &entry);
}

static int checkout_submodule(
	checkout_data *data,
	const git_diff_file *file)
{
	int error = 0;
	git_submodule *sm;

	/* Until submodules are supported, UPDATE_ONLY means do nothing here */
	if ((data->strategy & GIT_CHECKOUT_UPDATE_ONLY) != 0)
		return 0;

	if ((error = git_futils_mkdir(
			file->path, git_repository_workdir(data->repo),
			data->opts.dir_mode, GIT_MKDIR_PATH)) < 0)
		return error;

	if ((error = git_submodule_lookup(&sm, data->repo, file->path)) < 0)
		return error;

	/* TODO: Support checkout_strategy options.  Two circumstances:
	 * 1 - submodule already checked out, but we need to move the HEAD
	 *     to the new OID, or
	 * 2 - submodule not checked out and we should recursively check it out
	 *
	 * Checkout will not execute a pull on the submodule, but a clone
	 * command should probably be able to.  Do we need a submodule callback?
	 */

	/* update the index unless prevented */
	if ((data->strategy & GIT_CHECKOUT_DONT_UPDATE_INDEX) == 0) {
		struct stat st;

		git_buf_truncate(&data->path, data->workdir_len);
		if (git_buf_puts(&data->path, file->path) < 0)
			return -1;

		if ((error = p_stat(git_buf_cstr(&data->path), &st)) < 0) {
			giterr_set(
				GITERR_CHECKOUT, "Could not stat submodule %s\n", file->path);
			return error;
		}

		st.st_mode = GIT_FILEMODE_COMMIT;

		error = checkout_update_index(data, file, &st);
	}

	return error;
}

static void report_progress(
	checkout_data *data,
	const char *path)
{
	if (data->opts.progress_cb)
		data->opts.progress_cb(
			path, data->completed_steps, data->total_steps,
			data->opts.progress_payload);
}

static int checkout_safe_for_update_only(const char *path, mode_t expected_mode)
{
	struct stat st;

	if (p_lstat(path, &st) < 0) {
		/* if doesn't exist, then no error and no update */
		if (errno == ENOENT || errno == ENOTDIR)
			return 0;

		/* otherwise, stat error and no update */
		giterr_set(GITERR_OS, "Failed to stat file '%s'", path);
		return -1;
	}

	/* only safe for update if this is the same type of file */
	if ((st.st_mode & ~0777) == (expected_mode & ~0777))
		return 1;

	return 0;
}

static int checkout_blob(
	checkout_data *data,
	const git_diff_file *file)
{
	int error = 0;
	git_blob *blob;
	struct stat st;

	git_buf_truncate(&data->path, data->workdir_len);
	if (git_buf_puts(&data->path, file->path) < 0)
		return -1;

	if ((data->strategy & GIT_CHECKOUT_UPDATE_ONLY) != 0) {
		int rval = checkout_safe_for_update_only(
			git_buf_cstr(&data->path), file->mode);
		if (rval <= 0)
			return rval;
	}

	if ((error = git_blob_lookup(&blob, data->repo, &file->oid)) < 0)
		return error;

	if (S_ISLNK(file->mode))
		error = blob_content_to_link(
			&st, blob, git_buf_cstr(&data->path), data->can_symlink);
	else
		error = blob_content_to_file(
			&st, blob, git_buf_cstr(&data->path), file->mode, &data->opts);

	git_blob_free(blob);

	/* if we try to create the blob and an existing directory blocks it from
	 * being written, then there must have been a typechange conflict in a
	 * parent directory - suppress the error and try to continue.
	 */
	if ((data->strategy & GIT_CHECKOUT_ALLOW_CONFLICTS) != 0 &&
		(error == GIT_ENOTFOUND || error == GIT_EEXISTS))
	{
		giterr_clear();
		error = 0;
	}

	/* update the index unless prevented */
	if (!error && (data->strategy & GIT_CHECKOUT_DONT_UPDATE_INDEX) == 0)
		error = checkout_update_index(data, file, &st);

	/* update the submodule data if this was a new .gitmodules file */
	if (!error && strcmp(file->path, ".gitmodules") == 0)
		data->reload_submodules = true;

	return error;
}

static int checkout_remove_the_old(
	unsigned int *actions,
	checkout_data *data)
{
	int error = 0;
	git_diff_delta *delta;
	const char *str;
	size_t i;
	const char *workdir = git_buf_cstr(&data->path);
	uint32_t flg = GIT_RMDIR_EMPTY_PARENTS |
		GIT_RMDIR_REMOVE_FILES | GIT_RMDIR_REMOVE_BLOCKERS;

	git_buf_truncate(&data->path, data->workdir_len);

	git_vector_foreach(&data->diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__REMOVE) {
			error = git_futils_rmdir_r(delta->old_file.path, workdir, flg);
			if (error < 0)
				return error;

			data->completed_steps++;
			report_progress(data, delta->old_file.path);

			if ((actions[i] & CHECKOUT_ACTION__UPDATE_BLOB) == 0 &&
				(data->strategy & GIT_CHECKOUT_DONT_UPDATE_INDEX) == 0 &&
				data->index != NULL)
			{
				(void)git_index_remove(data->index, delta->old_file.path, 0);
			}
		}
	}

	git_vector_foreach(&data->removes, i, str) {
		error = git_futils_rmdir_r(str, workdir, flg);
		if (error < 0)
			return error;

		data->completed_steps++;
		report_progress(data, str);

		if ((data->strategy & GIT_CHECKOUT_DONT_UPDATE_INDEX) == 0 &&
			data->index != NULL)
		{
			if (str[strlen(str) - 1] == '/')
				(void)git_index_remove_directory(data->index, str, 0);
			else
				(void)git_index_remove(data->index, str, 0);
		}
	}

	return 0;
}

static int checkout_deferred_remove(git_repository *repo, const char *path)
{
#if 0
	int error = git_futils_rmdir_r(
		path, git_repository_workdir(repo), GIT_RMDIR_EMPTY_PARENTS);

	if (error == GIT_ENOTFOUND) {
		error = 0;
		giterr_clear();
	}

	return error;
#else
	GIT_UNUSED(repo);
	GIT_UNUSED(path);
	assert(false);
	return 0;
#endif
}

static int checkout_create_the_new(
	unsigned int *actions,
	checkout_data *data)
{
	int error = 0;
	git_diff_delta *delta;
	size_t i;

	git_vector_foreach(&data->diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__DEFER_REMOVE) {
			/* this had a blocker directory that should only be removed iff
			 * all of the contents of the directory were safely removed
			 */
			if ((error = checkout_deferred_remove(
					data->repo, delta->old_file.path)) < 0)
				return error;
		}

		if (actions[i] & CHECKOUT_ACTION__UPDATE_BLOB) {
			error = checkout_blob(data, &delta->new_file);
			if (error < 0)
				return error;

			data->completed_steps++;
			report_progress(data, delta->new_file.path);
		}
	}

	return 0;
}

static int checkout_create_submodules(
	unsigned int *actions,
	checkout_data *data)
{
	int error = 0;
	git_diff_delta *delta;
	size_t i;

	/* initial reload of submodules if .gitmodules was changed */
	if (data->reload_submodules &&
		(error = git_submodule_reload_all(data->repo)) < 0)
		return error;

	git_vector_foreach(&data->diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__DEFER_REMOVE) {
			/* this has a blocker directory that should only be removed iff
			 * all of the contents of the directory were safely removed
			 */
			if ((error = checkout_deferred_remove(
					data->repo, delta->old_file.path)) < 0)
				return error;
		}

		if (actions[i] & CHECKOUT_ACTION__UPDATE_SUBMODULE) {
			int error = checkout_submodule(data, &delta->new_file);
			if (error < 0)
				return error;

			data->completed_steps++;
			report_progress(data, delta->new_file.path);
		}
	}

	/* final reload once submodules have been updated */
	return git_submodule_reload_all(data->repo);
}

static int checkout_lookup_head_tree(git_tree **out, git_repository *repo)
{
	int error = 0;
	git_reference *ref = NULL;
	git_object *head;

	if (!(error = git_repository_head(&ref, repo)) &&
		!(error = git_reference_peel(&head, ref, GIT_OBJ_TREE)))
		*out = (git_tree *)head;

	git_reference_free(ref);

	return error;
}

static void checkout_data_clear(checkout_data *data)
{
	if (data->opts_free_baseline) {
		git_tree_free(data->opts.baseline);
		data->opts.baseline = NULL;
	}

	git_vector_free(&data->removes);
	git_pool_clear(&data->pool);

	git__free(data->pfx);
	data->pfx = NULL;

	git_buf_free(&data->path);

	git_index_free(data->index);
	data->index = NULL;
}

static int checkout_data_init(
	checkout_data *data,
	git_iterator *target,
	git_checkout_opts *proposed)
{
	int error = 0;
	git_config *cfg;
	git_repository *repo = git_iterator_owner(target);

	memset(data, 0, sizeof(*data));

	if (!repo) {
		giterr_set(GITERR_CHECKOUT, "Cannot checkout nothing");
		return -1;
	}

	if ((error = git_repository__ensure_not_bare(repo, "checkout")) < 0)
		return error;

	if ((error = git_repository_config__weakptr(&cfg, repo)) < 0)
		return error;

	data->repo = repo;

	GITERR_CHECK_VERSION(
		proposed, GIT_CHECKOUT_OPTS_VERSION, "git_checkout_opts");

	if (!proposed)
		GIT_INIT_STRUCTURE(&data->opts, GIT_CHECKOUT_OPTS_VERSION);
	else
		memmove(&data->opts, proposed, sizeof(git_checkout_opts));

	/* refresh config and index content unless NO_REFRESH is given */
	if ((data->opts.checkout_strategy & GIT_CHECKOUT_NO_REFRESH) == 0) {
		if ((error = git_config_refresh(cfg)) < 0)
			goto cleanup;

		/* if we are checking out the index, don't reload,
		 * otherwise get index and force reload
		 */
		if ((data->index = git_iterator_get_index(target)) != NULL) {
			GIT_REFCOUNT_INC(data->index);
		} else {
			/* otherwise, grab and reload the index */
			if ((error = git_repository_index(&data->index, data->repo)) < 0 ||
				(error = git_index_read(data->index)) < 0)
				goto cleanup;

			/* clear the REUC when doing a tree or commit checkout */
			git_index_reuc_clear(data->index);
		}
	}

	/* if you are forcing, definitely allow safe updates */
	if ((data->opts.checkout_strategy & GIT_CHECKOUT_FORCE) != 0)
		data->opts.checkout_strategy |= GIT_CHECKOUT_SAFE_CREATE;
	if ((data->opts.checkout_strategy & GIT_CHECKOUT_SAFE_CREATE) != 0)
		data->opts.checkout_strategy |= GIT_CHECKOUT_SAFE;

	data->strategy = data->opts.checkout_strategy;

	/* opts->disable_filters is false by default */

	if (!data->opts.dir_mode)
		data->opts.dir_mode = GIT_DIR_MODE;

	if (!data->opts.file_open_flags)
		data->opts.file_open_flags = O_CREAT | O_TRUNC | O_WRONLY;

	data->pfx = git_pathspec_prefix(&data->opts.paths);

	error = git_config_get_bool(&data->can_symlink, cfg, "core.symlinks");
	if (error < 0) {
		if (error != GIT_ENOTFOUND)
			goto cleanup;

		/* If "core.symlinks" is not found anywhere, default to true. */
		data->can_symlink = true;
		giterr_clear();
		error = 0;
	}

	if (!data->opts.baseline) {
		data->opts_free_baseline = true;
		error = checkout_lookup_head_tree(&data->opts.baseline, repo);

		if (error == GIT_EORPHANEDHEAD) {
			error = 0;
			giterr_clear();
		}

		if (error < 0)
			goto cleanup;
	}

	if ((error = git_vector_init(&data->removes, 0, git__strcmp_cb)) < 0 ||
		(error = git_pool_init(&data->pool, 1, 0)) < 0 ||
		(error = git_buf_puts(&data->path, git_repository_workdir(repo))) < 0)
		goto cleanup;

	data->workdir_len = git_buf_len(&data->path);

cleanup:
	if (error < 0)
		checkout_data_clear(data);

	return error;
}

int git_checkout_iterator(
	git_iterator *target,
	git_checkout_opts *opts)
{
	int error = 0;
	git_iterator *baseline = NULL, *workdir = NULL;
	checkout_data data = {0};
	git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
	uint32_t *actions = NULL;
	size_t *counts = NULL;
	git_iterator_flag_t iterflags = 0;

	/* initialize structures and options */
	error = checkout_data_init(&data, target, opts);
	if (error < 0)
		return error;

	diff_opts.flags =
		GIT_DIFF_INCLUDE_UNMODIFIED |
		GIT_DIFF_INCLUDE_UNTRACKED |
		GIT_DIFF_RECURSE_UNTRACKED_DIRS | /* needed to match baseline */
		GIT_DIFF_INCLUDE_IGNORED |
		GIT_DIFF_INCLUDE_TYPECHANGE |
		GIT_DIFF_INCLUDE_TYPECHANGE_TREES |
		GIT_DIFF_SKIP_BINARY_CHECK;
	if (data.opts.checkout_strategy & GIT_CHECKOUT_DISABLE_PATHSPEC_MATCH)
		diff_opts.flags |= GIT_DIFF_DISABLE_PATHSPEC_MATCH;
	if (data.opts.paths.count > 0)
		diff_opts.pathspec = data.opts.paths;

	/* set up iterators */

	iterflags = git_iterator_ignore_case(target) ?
		GIT_ITERATOR_IGNORE_CASE : GIT_ITERATOR_DONT_IGNORE_CASE;

	if ((error = git_iterator_reset(target, data.pfx, data.pfx)) < 0 ||
		(error = git_iterator_for_workdir(
			&workdir, data.repo, iterflags | GIT_ITERATOR_DONT_AUTOEXPAND,
			data.pfx, data.pfx)) < 0 ||
		(error = git_iterator_for_tree(
			&baseline, data.opts.baseline, iterflags, data.pfx, data.pfx)) < 0)
		goto cleanup;

	/* Should not have case insensitivity mismatch */
	assert(git_iterator_ignore_case(workdir) == git_iterator_ignore_case(baseline));

	/* Generate baseline-to-target diff which will include an entry for
	 * every possible update that might need to be made.
	 */
	if ((error = git_diff__from_iterators(
			&data.diff, data.repo, baseline, target, &diff_opts)) < 0)
		goto cleanup;

	/* Loop through diff (and working directory iterator) building a list of
	 * actions to be taken, plus look for conflicts and send notifications.
	 */
	if ((error = checkout_get_actions(&actions, &counts, &data, workdir)) < 0)
		goto cleanup;

	data.total_steps = counts[CHECKOUT_ACTION__REMOVE] +
		counts[CHECKOUT_ACTION__UPDATE_BLOB] +
		counts[CHECKOUT_ACTION__UPDATE_SUBMODULE];

	report_progress(&data, NULL); /* establish 0 baseline */

	/* To deal with some order dependencies, perform remaining checkout
	 * in three passes: removes, then update blobs, then update submodules.
	 */
	if (counts[CHECKOUT_ACTION__REMOVE] > 0 &&
		(error = checkout_remove_the_old(actions, &data)) < 0)
		goto cleanup;

	if (counts[CHECKOUT_ACTION__UPDATE_BLOB] > 0 &&
		(error = checkout_create_the_new(actions, &data)) < 0)
		goto cleanup;

	if (counts[CHECKOUT_ACTION__UPDATE_SUBMODULE] > 0 &&
		(error = checkout_create_submodules(actions, &data)) < 0)
		goto cleanup;

	assert(data.completed_steps == data.total_steps);

cleanup:
	if (error == GIT_EUSER)
		giterr_clear();

	if (!error && data.index != NULL &&
		(data.strategy & GIT_CHECKOUT_DONT_UPDATE_INDEX) == 0)
		error = git_index_write(data.index);

	git_diff_list_free(data.diff);
	git_iterator_free(workdir);
	git_iterator_free(baseline);
	git__free(actions);
	git__free(counts);
	checkout_data_clear(&data);

	return error;
}

int git_checkout_index(
	git_repository *repo,
	git_index *index,
	git_checkout_opts *opts)
{
	int error;
	git_iterator *index_i;

	if ((error = git_repository__ensure_not_bare(repo, "checkout index")) < 0)
		return error;

	if (!index && (error = git_repository_index__weakptr(&index, repo)) < 0)
		return error;
	GIT_REFCOUNT_INC(index);

	if (!(error = git_iterator_for_index(&index_i, index, 0, NULL, NULL)))
		error = git_checkout_iterator(index_i, opts);

	git_iterator_free(index_i);
	git_index_free(index);

	return error;
}

int git_checkout_tree(
	git_repository *repo,
	const git_object *treeish,
	git_checkout_opts *opts)
{
	int error;
	git_tree *tree = NULL;
	git_iterator *tree_i = NULL;

	if ((error = git_repository__ensure_not_bare(repo, "checkout tree")) < 0)
		return error;

	if (git_object_peel((git_object **)&tree, treeish, GIT_OBJ_TREE) < 0) {
		giterr_set(
			GITERR_CHECKOUT, "Provided object cannot be peeled to a tree");
		return -1;
	}

	if (!(error = git_iterator_for_tree(&tree_i, tree, 0, NULL, NULL)))
		error = git_checkout_iterator(tree_i, opts);

	git_iterator_free(tree_i);
	git_tree_free(tree);

	return error;
}

int git_checkout_head(
	git_repository *repo,
	git_checkout_opts *opts)
{
	int error;
	git_tree *head = NULL;
	git_iterator *head_i = NULL;

	if ((error = git_repository__ensure_not_bare(repo, "checkout head")) < 0)
		return error;

	if (!(error = checkout_lookup_head_tree(&head, repo)) &&
		!(error = git_iterator_for_tree(&head_i, head, 0, NULL, NULL)))
		error = git_checkout_iterator(head_i, opts);

	git_iterator_free(head_i);
	git_tree_free(head);

	return error;
}
