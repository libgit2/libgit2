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

struct checkout_diff_data
{
	git_buf *path;
	size_t workdir_len;
	git_checkout_opts *checkout_opts;
	git_indexer_stats *stats;
	git_repository *owner;
	bool can_symlink;
	bool found_submodules;
	bool create_submodules;
	int error;
};

static int buffer_to_file(
	git_buf *buffer,
	const char *path,
	mode_t dir_mode,
	int file_open_flags,
	mode_t file_mode)
{
	int fd, error, error_close;

	if ((error = git_futils_mkpath2file(path, dir_mode)) < 0)
		return error;

	if ((fd = p_open(path, file_open_flags, file_mode)) < 0)
		return fd;

	error = p_write(fd, git_buf_cstr(buffer), git_buf_len(buffer));

	error_close = p_close(fd);

	if (!error)
		error = error_close;

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

	error = buffer_to_file(&filtered, path, opts->dir_mode, opts->file_open_flags, file_mode);

cleanup:
	git_filters_free(&filters);
	git_buf_free(&unfiltered);
	if (!dont_free_filtered)
		git_buf_free(&filtered);

	return error;
}

static int blob_content_to_link(git_blob *blob, const char *path, bool can_symlink)
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
	struct checkout_diff_data *data,
	const git_diff_file *file)
{
	if (git_futils_mkdir(
			file->path, git_repository_workdir(data->owner),
			data->checkout_opts->dir_mode, GIT_MKDIR_PATH) < 0)
		return -1;

	/* TODO: two cases:
	 * 1 - submodule already checked out, but we need to move the HEAD
	 *     to the new OID, or
	 * 2 - submodule not checked out and we should recursively check it out
	 *
	 * Checkout will not execute a pull request on the submodule, but a
	 * clone command should probably be able to.  Do we need a submodule
	 * callback option?
	 */

	return 0;
}

static int checkout_blob(
	struct checkout_diff_data *data,
	const git_diff_file *file)
{
	git_blob *blob;
	int error;

	git_buf_truncate(data->path, data->workdir_len);
	if (git_buf_joinpath(data->path, git_buf_cstr(data->path), file->path) < 0)
		return -1;

	if ((error = git_blob_lookup(&blob, data->owner, &file->oid)) < 0)
		return error;

	if (S_ISLNK(file->mode))
		error = blob_content_to_link(
			blob, git_buf_cstr(data->path), data->can_symlink);
	else
		error = blob_content_to_file(
			blob, git_buf_cstr(data->path), file->mode, data->checkout_opts);

	git_blob_free(blob);

	return error;
}

static int checkout_remove_the_old(
	void *cb_data, const git_diff_delta *delta, float progress)
{
	struct checkout_diff_data *data = cb_data;
	git_checkout_opts *opts = data->checkout_opts;

	GIT_UNUSED(progress);
	data->stats->processed++;

	if ((delta->status == GIT_DELTA_UNTRACKED &&
		 (opts->checkout_strategy & GIT_CHECKOUT_REMOVE_UNTRACKED) != 0) ||
		(delta->status == GIT_DELTA_TYPECHANGE &&
		 (opts->checkout_strategy & GIT_CHECKOUT_OVERWRITE_MODIFIED) != 0))
	{
		data->error = git_futils_rmdir_r(
			delta->new_file.path,
			git_repository_workdir(data->owner),
			GIT_DIRREMOVAL_FILES_AND_DIRS);
	}

	return data->error;
}

static int checkout_create_the_new(
	void *cb_data, const git_diff_delta *delta, float progress)
{
	int error = 0;
	struct checkout_diff_data *data = cb_data;
	git_checkout_opts *opts = data->checkout_opts;
	bool do_checkout = false, do_notify = false;

	GIT_UNUSED(progress);
	data->stats->processed++;

	if (delta->status == GIT_DELTA_MODIFIED ||
		delta->status == GIT_DELTA_TYPECHANGE)
	{
		if ((opts->checkout_strategy & GIT_CHECKOUT_OVERWRITE_MODIFIED) != 0)
			do_checkout = true;
		else if (opts->skipped_notify_cb != NULL)
			do_notify = !data->create_submodules;
	}
	else if (delta->status == GIT_DELTA_DELETED &&
			 (opts->checkout_strategy & GIT_CHECKOUT_CREATE_MISSING) != 0)
		do_checkout = true;

	if (do_notify) {
		if (opts->skipped_notify_cb(
			delta->old_file.path, &delta->old_file.oid,
			delta->old_file.mode, opts->notify_payload))
		{
			giterr_clear();
			error = GIT_EUSER;
		}
	}

	if (do_checkout) {
		bool is_submodule = S_ISGITLINK(delta->old_file.mode);

		if (is_submodule)
			data->found_submodules = true;

		if (!is_submodule && !data->create_submodules)
			error = checkout_blob(data, &delta->old_file);

		else if (is_submodule && data->create_submodules)
			error = checkout_submodule(data, &delta->old_file);
	}

	if (error)
		data->error = error;

	return error;
}

static int retrieve_symlink_capabilities(git_repository *repo, bool *can_symlink)
{
	git_config *cfg;
	int error;

	if (git_repository_config__weakptr(&cfg, repo) < 0)
		return -1;

	error = git_config_get_bool((int *)can_symlink, cfg, "core.symlinks");

	/*
	 * When no "core.symlinks" entry is found in any of the configuration
	 * store (local, global or system), default value is "true".
	 */
	if (error == GIT_ENOTFOUND) {
		*can_symlink = true;
		error = 0;
	}

	return error;
}

static void normalize_options(git_checkout_opts *normalized, git_checkout_opts *proposed)
{
	assert(normalized);

	if (!proposed)
		memset(normalized, 0, sizeof(git_checkout_opts));
	else
		memmove(normalized, proposed, sizeof(git_checkout_opts));

	/* Default options */
	if (!normalized->checkout_strategy)
		normalized->checkout_strategy = GIT_CHECKOUT_DEFAULT;

	/* opts->disable_filters is false by default */
	if (!normalized->dir_mode)
		normalized->dir_mode = GIT_DIR_MODE;

	if (!normalized->file_open_flags)
		normalized->file_open_flags = O_CREAT | O_TRUNC | O_WRONLY;
}

int git_checkout_index(
	git_repository *repo,
	git_checkout_opts *opts,
	git_indexer_stats *stats)
{
	git_diff_list *diff = NULL;
	git_indexer_stats dummy_stats;

	git_diff_options diff_opts = {0};
	git_checkout_opts checkout_opts;

	struct checkout_diff_data data;
	git_buf workdir = GIT_BUF_INIT;

	int error;

	assert(repo);

	if ((error = git_repository__ensure_not_bare(repo, "checkout")) < 0)
		return error;

	diff_opts.flags =
		GIT_DIFF_INCLUDE_UNTRACKED |
		GIT_DIFF_INCLUDE_TYPECHANGE |
		GIT_DIFF_SKIP_BINARY_CHECK;

	if (opts && opts->paths.count > 0)
		diff_opts.pathspec = opts->paths;

	if ((error = git_diff_workdir_to_index(repo, &diff_opts, &diff)) < 0)
		goto cleanup;

	if ((error = git_buf_puts(&workdir, git_repository_workdir(repo))) < 0)
		goto cleanup;

	normalize_options(&checkout_opts, opts);

	if (!stats)
		stats = &dummy_stats;

	stats->processed = 0;
	/* total based on 3 passes, but it might be 2 if no submodules */
	stats->total = (unsigned int)git_diff_num_deltas(diff) * 3;

	memset(&data, 0, sizeof(data));

	data.path = &workdir;
	data.workdir_len = git_buf_len(&workdir);
	data.checkout_opts = &checkout_opts;
	data.stats = stats;
	data.owner = repo;

	if ((error = retrieve_symlink_capabilities(repo, &data.can_symlink)) < 0)
		goto cleanup;

	/* Checkout is best performed with three passes through the diff.
	 *
	 * 1. First do removes, because we iterate in alphabetical order, thus
	 *    a new untracked directory will end up sorted *after* a blob that
	 *    should be checked out with the same name.
	 * 2. Then checkout all blobs.
	 * 3. Then checkout all submodules in case a new .gitmodules blob was
	 *    checked out during pass #2.
	 */

	if (!(error = git_diff_foreach(
			diff, &data, checkout_remove_the_old, NULL, NULL)) &&
		!(error = git_diff_foreach(
			diff, &data, checkout_create_the_new, NULL, NULL)) &&
		data.found_submodules)
	{
		data.create_submodules = true;
		error = git_diff_foreach(
			diff, &data, checkout_create_the_new, NULL, NULL);
	}

	stats->processed = stats->total;

cleanup:
	if (error == GIT_EUSER)
		error = (data.error != 0) ? data.error : -1;

	git_diff_list_free(diff);
	git_buf_free(&workdir);

	return error;
}

int git_checkout_tree(
	git_repository *repo,
	git_object *treeish,
	git_checkout_opts *opts,
	git_indexer_stats *stats)
{
	git_index *index = NULL;
	git_tree *tree = NULL;

	int error;

	assert(repo && treeish);

	if (git_object_peel((git_object **)&tree, treeish, GIT_OBJ_TREE) < 0) {
		giterr_set(GITERR_INVALID, "Provided treeish cannot be peeled into a tree.");
		return GIT_ERROR;
	}

	if ((error = git_repository_index(&index, repo)) < 0)
		goto cleanup;

	if ((error = git_index_read_tree(index, tree, NULL)) < 0)
		goto cleanup;

	if ((error = git_index_write(index)) < 0)
		goto cleanup;

	error = git_checkout_index(repo, opts, stats);

cleanup:
	git_index_free(index);
	git_tree_free(tree);
	return error;
}

int git_checkout_head(
	git_repository *repo,
	git_checkout_opts *opts,
	git_indexer_stats *stats)
{
	int error;
	git_tree *tree = NULL;

	assert(repo);

	if (git_repository_head_tree(&tree, repo) < 0)
		return -1;

	error = git_checkout_tree(repo, (git_object *)tree, opts, stats);

	git_tree_free(tree);

	return error;
}
