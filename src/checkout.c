/*
 * Copyright (C) 2009-2012 the libgit2 contributors
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

#include "refs.h"
#include "repository.h"
#include "filter.h"
#include "blob.h"
#include "diff.h"
#include "pathspec.h"

/* Key
 * ===
 * B1,B2,B3 - blobs with different SHAs,
 * Bi       - ignored blob (WD only)
 * T1,T2,T3 - trees with different SHAs,
 * Ti       - ignored tree (WD only)
 * x        - nothing
 */

/* Diff with 2 non-workdir iterators
 * =================================
 *    Old New
 *    --- ---
 *  0   x   x - nothing
 *  1   x  B1 - added blob
 *  2   x  T1 - added tree
 *  3  B1   x - removed blob
 *  4  B1  B1 - unmodified blob
 *  5  B1  B2 - modified blob
 *  6  B1  T1 - typechange blob -> tree
 *  7  T1   x - removed tree
 *  8  T1  B1 - typechange tree -> blob
 *  9  T1  T1 - unmodified tree
 * 10  T1  T2 - modified tree (implies modified/added/removed blob inside)
 */

/* Diff with non-work & workdir iterators
 * ======================================
 *    Old New-WD
 *    --- ------
 *  0   x   x - nothing
 *  1   x  B1 - added blob
 *  2   x  Bi - ignored file
 *  3   x  T1 - added tree
 *  4   x  Ti - ignored tree
 *  5  B1   x - removed blob
 *  6  B1  B1 - unmodified blob
 *  7  B1  B2 - modified blob
 *  8  B1  T1 - typechange blob -> tree
 *  9  B1  Ti - removed blob AND ignored tree as separate items
 * 10  T1   x - removed tree
 * 11  T1  B1 - typechange tree -> blob
 * 12  T1  Bi - removed tree AND ignored blob as separate items
 * 13  T1  T1 - unmodified tree
 * 14  T1  T2 - modified tree (implies modified/added/removed blob inside)
 *
 * If there is a corresponding blob in the old, Bi is irrelevant
 * If there is a corresponding tree in the old, Ti is irrelevant
 */

/* Checkout From 3 Iterators (2 not workdir, 1 workdir)
 * ====================================================
 *
 * (Expect == Old HEAD / Desire == What To Checkout / Actual == Workdir)
 *
 *    Expect Desire Actual-WD
 *    ------ ------ ------
 *  0      x      x      x - nothing
 *  1      x      x B1/Bi/T1/Ti - untracked/ignored blob/tree (SAFE)
 *  2+     x     B1      x - add blob (SAFE)
 *  3      x     B1     B1 - independently added blob (FORCEABLE-2)
 *  4*     x     B1 B2/Bi/T1/Ti - add blob with content conflict (FORCEABLE-2)
 *  5+     x     T1      x - add tree (SAFE)
 *  6*     x     T1  B1/Bi - add tree with blob conflict (FORCEABLE-2)
 *  7      x     T1   T1/i - independently added tree (SAFE+MISSING)
 *  8     B1      x      x - independently deleted blob (SAFE+MISSING)
 *  9-    B1      x     B1 - delete blob (SAFE)
 * 10-    B1      x     B2 - delete of modified blob (FORCEABLE-1)
 * 11     B1      x  T1/Ti - independently deleted blob AND untrack/ign tree (SAFE+MISSING !!!)
 * 12     B1     B1      x - locally deleted blob (DIRTY || SAFE+CREATE)
 * 13+    B1     B2      x - update to deleted blob (SAFE+MISSING)
 * 14     B1     B1     B1 - unmodified file (SAFE)
 * 15     B1     B1     B2 - locally modified file (DIRTY)
 * 16+    B1     B2     B1 - update unmodified blob (SAFE)
 * 17     B1     B2     B2 - independently updated blob (FORCEABLE-1)
 * 18+    B1     B2     B3 - update to modified blob (FORCEABLE-1)
 * 19     B1     B1  T1/Ti - locally deleted blob AND untrack/ign tree (DIRTY)
 * 20*    B1     B2  T1/Ti - update to deleted blob AND untrack/ign tree (F-1)
 * 21+    B1     T1      x - add tree with locally deleted blob (SAFE+MISSING)
 * 22*    B1     T1     B1 - add tree AND deleted blob (SAFE)
 * 23*    B1     T1     B2 - add tree with delete of modified blob (F-1)
 * 24     B1     T1     T1 - add tree with deleted blob (F-1)
 * 25     T1      x      x - independently deleted tree (SAFE+MISSING)
 * 26     T1      x  B1/Bi - independently deleted tree AND untrack/ign blob (F-1)
 * 27-    T1      x     T1 - deleted tree (MAYBE SAFE)
 * 28+    T1     B1      x - deleted tree AND added blob (SAFE+MISSING)
 * 29     T1     B1     B1 - independently typechanged tree -> blob (F-1)
 * 30+    T1     B1     B2 - typechange tree->blob with conflicting blob (F-1)
 * 31*    T1     B1  T1/T2 - typechange tree->blob (MAYBE SAFE)
 * 32+    T1     T1      x - restore locally deleted tree (SAFE+MISSING)
 * 33     T1     T1  B1/Bi - locally typechange tree->untrack/ign blob (DIRTY)
 * 34     T1     T1  T1/T2 - unmodified tree (MAYBE SAFE)
 * 35+    T1     T2      x - update locally deleted tree (SAFE+MISSING)
 * 36*    T1     T2  B1/Bi - update to tree with typechanged tree->blob conflict (F-1)
 * 37     T1     T2 T1/T2/T3 - update to existing tree (MAYBE SAFE)
 *
 * The number will be followed by ' ' if no change is needed or '+' if the
 * case needs to write to disk or '-' if something must be deleted and '*'
 * if there should be a delete followed by an write.
 *
 * There are four tiers of safe cases:
 * - SAFE         == completely safe to update
 * - SAFE+MISSING == safe except the workdir is missing the expect content
 * - MAYBE SAFE   == safe if workdir tree matches (or is missing) expected
 *                   content, which is unknown at this point
 * - FORCEABLE == conflict unless FORCE is given
 * - DIRTY     == no conflict but change is not applied unless FORCE
 *
 * Some slightly unusual circumstances:
 *  8 - parent dir is only deleted when file is, so parent will be left if
 *      empty even though it would be deleted if the file were present
 * 11 - core git does not consider this a conflict but attempts to delete T1
 *      and gives "unable to unlink file" error yet does not skip the rest
 *      of the operation
 * 12 - without FORCE file is left deleted (i.e. not restored) so new wd is
 *      dirty (and warning message "D file" is printed), with FORCE, file is
 *      restored.
 * 24 - This should be considered MAYBE SAFE since effectively it is 7 and 8
 *      combined, but core git considers this a conflict unless forced.
 * 26 - This combines two cases (1 & 25) (and also implied 8 for tree content)
 *      which are ok on their own, but core git treat this as a conflict.
 *      If not forced, this is a conflict.  If forced, this actually doesn't
 *      have to write anything and leaves the new blob as an untracked file.
 * 32 - This is the only case where the expected and desired values match
 *      and yet we will still write to the working directory.  In all other
 *      cases, if expected == desired, we don't touch the workdir (it is
 *      either already right or is "dirty").  However, since this case also
 *      implies that a ?/B1/x case will exist as well, it can be skipped.
 *
 * Cases 3, 17, 24, 26, and 29 are all considered conflicts even though
 * none of them will require making any updates to the working directory.
 */

/*    expect desire  wd
 *  1    x      x     T -> ignored dir OR untracked dir OR parent dir
 *  2    x      x     I -> ignored file
 *  3    x      x     A -> untracked file
 *  4    x      A     x -> add from index (no conflict)
 *  5    x      A     A -> independently added file
 *  6    x      A     B -> add with conflicting file
 *  7    A      x     x -> independently deleted file
 *  8    A      x     A -> delete from index (no conflict)
 *  9    A      x     B -> delete of modified file
 * 10    A      A     x -> locally deleted file
 * 11    A      A     A -> unmodified file (no conflict)
 * 12    A      A     B -> locally modified
 * 13    A      B     x -> update of deleted file
 * 14    A      B     A -> update of unmodified file (no conflict)
 * 15    A      B     B -> independently updated file
 * 16    A      B     C -> update of modified file
 */

enum {
	CHECKOUT_ACTION__NONE = 0,
	CHECKOUT_ACTION__REMOVE = 1,
	CHECKOUT_ACTION__UPDATE_BLOB = 2,
	CHECKOUT_ACTION__UPDATE_SUBMODULE = 4,
	CHECKOUT_ACTION__CONFLICT = 8,
	CHECKOUT_ACTION__MAX = 8,
	CHECKOUT_ACTION__REMOVE_EMPTY = 16,
};

typedef struct {
	git_repository *repo;
	git_diff_list *diff;
	git_checkout_opts *opts;
	const char *pfx;
	git_buf *path;
	size_t workdir_len;
	bool can_symlink;
	int error;
	size_t total_steps;
	size_t completed_steps;
} checkout_diff_data;

static int checkout_notify(
	checkout_diff_data *data,
	git_checkout_notify_t why,
	const git_diff_delta *delta,
	const git_index_entry *wditem)
{
	GIT_UNUSED(data);
	GIT_UNUSED(why);
	GIT_UNUSED(delta);
	GIT_UNUSED(wditem);
	return 0;
}

static bool checkout_is_workdir_modified(
	checkout_diff_data *data,
	const git_diff_file *item,
	const git_index_entry *wditem)
{
	git_oid oid;

	if (item->size != wditem->file_size)
		return true;

	if (git_diff__oid_for_file(
			data->repo, wditem->path, wditem->mode,
			wditem->file_size, &oid) < 0)
		return false;

	return (git_oid_cmp(&item->oid, &oid) != 0);
}

static int checkout_action_for_delta(
	checkout_diff_data *data,
	const git_diff_delta *delta,
	const git_index_entry *wditem)
{
	int action = CHECKOUT_ACTION__NONE;
	unsigned int strat = data->opts->checkout_strategy;
	int safe = ((strat & GIT_CHECKOUT_SAFE) != 0) ?
		CHECKOUT_ACTION__UPDATE_BLOB : CHECKOUT_ACTION__NONE;
	int force = ((strat & GIT_CHECKOUT_FORCE) != 0) ?
		CHECKOUT_ACTION__UPDATE_BLOB : CHECKOUT_ACTION__CONFLICT;

	/* nothing in workdir, so this is pretty easy */
	if (!wditem) {
		switch (delta->status) {
		case GIT_DELTA_UNMODIFIED: /* case 12 */
			if ((strat & GIT_CHECKOUT_SAFE_CREATE) != 0)
				action = CHECKOUT_ACTION__UPDATE_BLOB;

			if (checkout_notify(data, GIT_CHECKOUT_NOTIFY_DIRTY, delta, NULL))
				return GIT_EUSER;
			break;
		case GIT_DELTA_ADDED:    /* case 2 or 28 (and 5 but not really) */
		case GIT_DELTA_MODIFIED: /* case 13 (and 35 but not really) */
			action = safe;
			break;
		case GIT_DELTA_TYPECHANGE: /* case 21 (B->T) and 28 (T->B)*/
			if (!S_ISDIR(delta->new_file.mode))
				action = safe;
			break;
		case GIT_DELTA_DELETED: /* case 8 or 25 */
		default: /* impossible */ break;
		}
	}

	/* workdir has a directory where this entry should be */
	else if (S_ISDIR(wditem->mode)) {
		switch (delta->status) {
		case GIT_DELTA_UNMODIFIED: /* case 19 or 24 (or 34 but not really) */
			if (checkout_notify(data, GIT_CHECKOUT_NOTIFY_DIRTY, delta, NULL) ||
				checkout_notify(
					data, GIT_CHECKOUT_NOTIFY_UNTRACKED, NULL, wditem))
				return GIT_EUSER;
			break;
		case GIT_DELTA_ADDED:/* case 4 (and 7 for dir) */
		case GIT_DELTA_MODIFIED: /* case 20 (or 37 but not really) */
			if (!S_ISDIR(delta->new_file.mode))
				action = force;
			break;
		case GIT_DELTA_DELETED: /* case 11 (and 27 for dir) */
			if (!S_ISDIR(delta->old_file.mode) &&
				checkout_notify(
					data, GIT_CHECKOUT_NOTIFY_UNTRACKED, NULL, wditem))
				return GIT_EUSER;
			break;
		case GIT_DELTA_TYPECHANGE: /* case 24 or 31 */
			/* For typechange to dir, dir is already created so no action */

			/* For typechange to blob, remove dir and add blob, but it is
			 * not safe to remove dir if it contains modified files.
			 * However, safely removing child files will remove the parent
			 * directory if is it left empty, so we only need to remove dir
			 * if it is already empty and has no children to remove.
			 */
			if (S_ISDIR(delta->old_file.mode)) {
				action = safe;
				if (action != 0)
					action |= CHECKOUT_ACTION__REMOVE |
						CHECKOUT_ACTION__REMOVE_EMPTY;
			}
			break;
		default: /* impossible */ break;
		}
	}

	/* workdir has a blob (or submodule) */
	else {
		switch (delta->status) {
		case GIT_DELTA_UNMODIFIED: /* case 14/15 or 33 */
			if (S_ISDIR(delta->old_file.mode) ||
				checkout_is_workdir_modified(data, &delta->old_file, wditem))
			{
				if (checkout_notify(
						data, GIT_CHECKOUT_NOTIFY_DIRTY, delta, wditem))
					return GIT_EUSER;

				if (force)
					action = CHECKOUT_ACTION__UPDATE_BLOB;
			}
			break;
		case GIT_DELTA_ADDED: /* case 3, 4 or 6 */
			action = force;
			break;
		case GIT_DELTA_DELETED: /* case 9 or 10 (or 26 but not really) */
			if (checkout_is_workdir_modified(data, &delta->old_file, wditem))
				action = force ?
					CHECKOUT_ACTION__REMOVE : CHECKOUT_ACTION__CONFLICT;
			else
				action = safe ?
					CHECKOUT_ACTION__REMOVE : CHECKOUT_ACTION__NONE;
			break;
		case GIT_DELTA_MODIFIED: /* case 16, 17, 18 (or 36 but not really) */
			if (checkout_is_workdir_modified(data, &delta->old_file, wditem))
				action = force;
			else
				action = safe;
			break;
		case GIT_DELTA_TYPECHANGE: /* case 22, 23, 29, 30 */
			if (S_ISDIR(delta->old_file.mode) ||
				checkout_is_workdir_modified(data, &delta->old_file, wditem))
				action = force;
			else
				action = safe;
			break;
		default: /* impossible */ break;
		}
	}

	if (action > 0 && (strat & GIT_CHECKOUT_UPDATE_ONLY) != 0)
		action = (action & ~CHECKOUT_ACTION__REMOVE);

	if (action > 0 && (action & CHECKOUT_ACTION__UPDATE_BLOB) != 0) {
		if (S_ISGITLINK(delta->new_file.mode))
			action = (action & ~CHECKOUT_ACTION__UPDATE_BLOB) |
				CHECKOUT_ACTION__UPDATE_SUBMODULE;

		if (checkout_notify(data, GIT_CHECKOUT_NOTIFY_UPDATED, delta, wditem))
			return GIT_EUSER;
	}

	if ((action & CHECKOUT_ACTION__CONFLICT) != 0) {
		if (checkout_notify(
				data, GIT_CHECKOUT_NOTIFY_CONFLICTS, delta, wditem))
			return GIT_EUSER;
	}

	return action;
}

static int checkout_track_wd(
	int *cmp_out,
	const git_index_entry **wditem_ptr,
	checkout_diff_data *data,
	git_iterator *actual,
	git_diff_delta *delta,
	git_vector *pathspec)
{
	int cmp = -1;
	const git_index_entry *wditem = *wditem_ptr;

	while (wditem) {
		bool notify = false;

		cmp = data->diff->strcomp(delta->new_file.path, wditem->path);
		if (cmp >= 0)
			break;

		if (!git_pathspec_match_path(
				pathspec, wditem->path, false, actual->ignore_case))
			notify = false;

		else if (S_ISDIR(wditem->mode)) {
			cmp = data->diff->pfxcomp(delta->new_file.path, wditem->path);

			if (cmp < 0)
				notify = true; /* notify untracked/ignored tree */
			else if (!cmp) {
				/* workdir is prefix of current, so dive in and continue */
				if (git_iterator_advance_into_directory(actual, &wditem) < 0)
					return -1;
				continue;
			}
			else /* how can the wditem->path be < 0 but a prefix be > 0 */
				assert(false);
		} else
			notify = true; /* notify untracked/ignored blob */

		if (notify && checkout_notify(
				data, git_iterator_current_is_ignored(actual) ?
				GIT_CHECKOUT_NOTIFY_IGNORED : GIT_CHECKOUT_NOTIFY_UNTRACKED,
				NULL, wditem))
			return GIT_EUSER;

		if (git_iterator_advance(actual, wditem_ptr) < 0)
			break;

		wditem = *wditem_ptr;
		cmp = -1;
	}

	*cmp_out = cmp;

	return 0;
}

static int checkout_get_actions(
	uint32_t **actions_ptr,
	size_t **counts_ptr,
	checkout_diff_data *data)
{
	int error = 0;
	git_iterator *actual = NULL;
	const git_index_entry *wditem;
	git_vector pathspec = GIT_VECTOR_INIT, *deltas;
	git_pool pathpool = GIT_POOL_INIT_STRINGPOOL;
	git_diff_delta *delta;
	size_t i, *counts = NULL;
	uint32_t *actions = NULL;
	bool allow_conflicts =
		((data->opts->checkout_strategy & GIT_CHECKOUT_ALLOW_CONFLICTS) != 0);

	if (data->opts->paths.count > 0 &&
		git_pathspec_init(&pathspec, &data->opts->paths, &pathpool) < 0)
		return -1;

	if ((error = git_iterator_for_workdir_range(
			&actual, data->repo, data->pfx, data->pfx)) < 0 ||
		(error = git_iterator_current(actual, &wditem)) < 0)
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
		int cmp = -1, act;

		/* move workdir iterator to follow along with deltas */
		if (wditem != NULL &&
			(error = checkout_track_wd(
				&cmp, &wditem, data, actual, delta, &pathspec)) < 0)
			goto fail;

		act = checkout_action_for_delta(data, delta, !cmp ? wditem : NULL);
		if (act < 0) {
			error = act;
			goto fail;
		}

		if (!cmp && git_iterator_advance(actual, &wditem) < 0)
			wditem = NULL;

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

	if (counts[CHECKOUT_ACTION__CONFLICT] > 0 && !allow_conflicts) {
		giterr_set(GITERR_CHECKOUT, "%d conflicts prevent checkout",
			(int)counts[CHECKOUT_ACTION__CONFLICT]);
		error = -1;
		goto fail;
	}

	git_iterator_free(actual);
	git_pathspec_free(&pathspec);
	git_pool_clear(&pathpool);

	return 0;

fail:
	*counts_ptr = NULL;
	git__free(counts);
	*actions_ptr = NULL;
	git__free(actions);

	git_iterator_free(actual);
	git_pathspec_free(&pathspec);
	git_pool_clear(&pathpool);

	return error;
}

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

static int checkout_remove_the_old(
	unsigned int *actions,
	checkout_diff_data *data)
{
	int error = 0;
	git_diff_delta *delta;
	size_t i;
	const char *workdir = git_buf_cstr(data->path);

	git_buf_truncate(data->path, data->workdir_len);

	git_vector_foreach(&data->diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__REMOVE) {
			uint32_t flg = GIT_RMDIR_EMPTY_PARENTS;
			bool empty_only =
				((actions[i] & CHECKOUT_ACTION__REMOVE_EMPTY) != 0);

			if (!empty_only)
				flg |= GIT_RMDIR_REMOVE_FILES | GIT_RMDIR_REMOVE_BLOCKERS;

			error = git_futils_rmdir_r(delta->old_file.path, workdir, flg);

			/* ignore error if empty_only, because that just means we lacked
			 * info to do the right thing when the action was picked.
			 */
			if (error < 0 && !empty_only)
				return error;

			data->completed_steps++;
			report_progress(data, delta->old_file.path);
		}
	}

	return 0;
}

static int checkout_create_the_new(
	unsigned int *actions,
	checkout_diff_data *data)
{
	git_diff_delta *delta;
	size_t i;

	git_vector_foreach(&data->diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__UPDATE_BLOB) {
			int error = checkout_blob(data, &delta->new_file);
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
	checkout_diff_data *data)
{
	git_diff_delta *delta;
	size_t i;

	git_vector_foreach(&data->diff->deltas, i, delta) {
		if (actions[i] & CHECKOUT_ACTION__UPDATE_SUBMODULE) {
			int error = checkout_submodule(data, &delta->new_file);
			if (error < 0)
				return error;

			data->completed_steps++;
			report_progress(data, delta->new_file.path);
		}
	}

	return 0;
}

static int retrieve_symlink_caps(git_repository *repo, bool *out)
{
	git_config *cfg;
	int error, can_symlink = 0;

	if (git_repository_config__weakptr(&cfg, repo) < 0)
		return -1;

	error = git_config_get_bool(&can_symlink, cfg, "core.symlinks");

	/* If "core.symlinks" is not found anywhere, default to true. */
	if (error == GIT_ENOTFOUND) {
		can_symlink = true;
		error = 0;
	}

	*out = can_symlink;

	return error;
}

int git_checkout__from_iterators(
	git_iterator *desired,
	git_iterator *expected,
	git_checkout_opts *opts,
	const char *pathspec_pfx)
{
	int error = 0;
	checkout_diff_data data;
	git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
	git_buf workdir = GIT_BUF_INIT;
	uint32_t *actions = NULL;
	size_t *counts = NULL;

	memset(&data, 0, sizeof(data));

	data.repo = git_iterator_owner(desired);
	if (!data.repo) data.repo = git_iterator_owner(expected);
	if (!data.repo) {
		giterr_set(GITERR_CHECKOUT, "Cannot checkout nothing");
		return -1;
	}

	diff_opts.flags =
		GIT_DIFF_INCLUDE_UNMODIFIED | GIT_DIFF_INCLUDE_UNTRACKED |
		GIT_DIFF_INCLUDE_TYPECHANGE | GIT_DIFF_SKIP_BINARY_CHECK;
	if (opts->paths.count > 0)
		diff_opts.pathspec = opts->paths;

	/* By analyzing the cases above, it becomes clear that checkout can work
	 * off the diff between the desired and expected trees, instead of using
	 * a work dir diff.  This should make things somewhat faster...
	 */
	if ((error = git_diff__from_iterators(
			&data.diff, data.repo, expected, desired, &diff_opts)) < 0)
		goto cleanup;

	if ((error = git_buf_puts(&workdir, git_repository_workdir(data.repo))) < 0)
		goto cleanup;

	data.opts = opts;
	data.pfx = pathspec_pfx;
	data.path = &workdir;
	data.workdir_len = git_buf_len(&workdir);

	/* In order to detect conflicts prior to performing any operations,
	 * and in order to deal with some order dependencies, checkout is best
	 * performed with up to four passes through the diff.
	 *
	 * 0. Figure out the actions to be taken,
	 * 1. Remove any files / directories as needed (because alphabetical
	 *    iteration means that an untracked directory will end up sorted
	 *    *after* a blob that should be checked out with the same name),
	 * 2. Then update all blobs,
	 * 3. Then update all submodules in case a new .gitmodules blob was
	 *    checked out during pass #2.
	 */
	if ((error = checkout_get_actions(&actions, &counts, &data)) < 0)
		goto cleanup;

	data.total_steps = counts[CHECKOUT_ACTION__REMOVE] +
		counts[CHECKOUT_ACTION__UPDATE_BLOB] +
		counts[CHECKOUT_ACTION__UPDATE_SUBMODULE];

	if ((error = retrieve_symlink_caps(data.repo, &data.can_symlink)) < 0)
		goto cleanup;

	report_progress(&data, NULL); /* establish 0 baseline */

	/* TODO: add ability to update index entries while checking out */

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

	git_diff_list_free(data.diff);
	git_buf_free(&workdir);
	git__free(actions);
	git__free(counts);

	return error;
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

static int checkout_normalize_opts(
	git_checkout_opts *normalized,
	char **pfx,
	git_repository *repo,
	git_checkout_opts *proposed)
{
	assert(normalized);

	GITERR_CHECK_VERSION(
		proposed, GIT_CHECKOUT_OPTS_VERSION, "git_checkout_opts");

	if (!proposed)
		GIT_INIT_STRUCTURE(normalized, GIT_CHECKOUT_OPTS_VERSION);
	else
		memmove(normalized, proposed, sizeof(git_checkout_opts));

	/* if you are forcing, definitely allow safe updates */

	if ((normalized->checkout_strategy & GIT_CHECKOUT_FORCE) != 0)
		normalized->checkout_strategy |= GIT_CHECKOUT_SAFE_CREATE;
	if ((normalized->checkout_strategy & GIT_CHECKOUT_SAFE_CREATE) != 0)
		normalized->checkout_strategy |= GIT_CHECKOUT_SAFE;

	/* opts->disable_filters is false by default */

	if (!normalized->dir_mode)
		normalized->dir_mode = GIT_DIR_MODE;

	if (!normalized->file_open_flags)
		normalized->file_open_flags = O_CREAT | O_TRUNC | O_WRONLY;

	if (pfx)
		*pfx = git_pathspec_prefix(&normalized->paths);

	if (!normalized->baseline) {
		normalized->checkout_strategy |= GIT_CHECKOUT__FREE_BASELINE;

		return checkout_lookup_head_tree(&normalized->baseline, repo);
	}

	return 0;
}

static void checkout_cleanup_opts(git_checkout_opts *opts)
{
	if ((opts->checkout_strategy & GIT_CHECKOUT__FREE_BASELINE) != 0)
		git_tree_free(opts->baseline);
}

int git_checkout_index(
	git_repository *repo,
	git_index *index,
	git_checkout_opts *opts)
{
	int error;
	git_checkout_opts co_opts;
	git_iterator *base_i, *index_i;
	char *pfx;

	assert(repo);

	GITERR_CHECK_VERSION(opts, GIT_CHECKOUT_OPTS_VERSION, "git_checkout_opts");

	if ((error = git_repository__ensure_not_bare(repo, "checkout index")) < 0)
		return error;

	if (!index && (error = git_repository_index__weakptr(&index, repo)) < 0)
		return error;

	if (!(error = checkout_normalize_opts(&co_opts, &pfx, repo, opts)) &&
		!(error = git_iterator_for_tree_range(
			  &base_i, co_opts.baseline, pfx, pfx)) &&
		!(error = git_iterator_for_index_range(&index_i, index, pfx, pfx)))
		error = git_checkout__from_iterators(index_i, base_i, &co_opts, pfx);

	git__free(pfx);
	git_iterator_free(index_i);
	git_iterator_free(base_i);
	checkout_cleanup_opts(&co_opts);

	return error;
}

int git_checkout_tree(
	git_repository *repo,
	const git_object *treeish,
	git_checkout_opts *opts)
{
	int error;
	git_checkout_opts co_opts;
	git_tree *tree;
	git_iterator *tree_i, *base_i;
	char *pfx;

	assert(repo);

	if ((error = git_repository__ensure_not_bare(repo, "checkout tree")) < 0)
		return error;

	if (git_object_peel((git_object **)&tree, treeish, GIT_OBJ_TREE) < 0) {
		giterr_set(
			GITERR_CHECKOUT, "Provided object cannot be peeled to a tree");
		return -1;
	}

	if (!(error = checkout_normalize_opts(&co_opts, &pfx, repo, opts)) &&
		!(error = git_iterator_for_tree_range(
			  &base_i, co_opts.baseline, pfx, pfx)) &&
		!(error = git_iterator_for_tree_range(&tree_i, tree, pfx, pfx)))
		error = git_checkout__from_iterators(tree_i, base_i, &co_opts, pfx);

	git__free(pfx);
	git_iterator_free(tree_i);
	git_iterator_free(base_i);
	git_tree_free(tree);
	checkout_cleanup_opts(&co_opts);

	return error;
}

int git_checkout_head(
	git_repository *repo,
	git_checkout_opts *opts)
{
	int error;
	git_checkout_opts co_opts;
	git_tree *head;
	git_iterator *i1, *i2;
	char *pfx;

	assert(repo);

	if ((error = git_repository__ensure_not_bare(repo, "checkout head")) < 0)
		return error;

	if ((error = checkout_lookup_head_tree(&head, repo)) < 0)
		return error;

	if (!(error = checkout_normalize_opts(&co_opts, &pfx, repo, opts)) &&
		!(error = git_iterator_for_tree_range(
			  &i1, co_opts.baseline, pfx, pfx)) &&
		!(error = git_iterator_for_tree_range(&i2, head, pfx, pfx)))
		error = git_checkout__from_iterators(i1, i2, &co_opts, pfx);

	git__free(pfx);
	git_iterator_free(i1);
	git_iterator_free(i2);
	git_tree_free(head);
	checkout_cleanup_opts(&co_opts);

	return error;
}
