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
	int workdir_len;
	git_checkout_opts *checkout_opts;
	git_indexer_stats *stats;
	git_repository *owner;
	bool can_symlink;
};

static int buffer_to_file(
	git_buf *buffer,
	const char *path,
	int dir_mode,
	int file_open_flags,
	mode_t file_mode)
{
	int fd, error_write, error_close;

	if (git_futils_mkpath2file(path, dir_mode) < 0)
		return -1;

	if ((fd = p_open(path, file_open_flags, file_mode)) < 0)
		return -1;

	error_write = p_write(fd, git_buf_cstr(buffer), git_buf_len(buffer));
	error_close = p_close(fd);

	return error_write ? error_write : error_close;
}

static int blob_content_to_file(
	git_blob *blob,
	const char *path,
	unsigned int entry_filemode,
	git_checkout_opts *opts)
{
	int error, nb_filters = 0, file_mode = opts->file_mode;
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
		if (git_blob__getbuf(&unfiltered, blob) < 0)
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

	if (git_blob__getbuf(&linktarget, blob) < 0)
		return -1;

	if (can_symlink)
		error = p_symlink(git_buf_cstr(&linktarget), path);
	else
		error = git_futils_fake_symlink(git_buf_cstr(&linktarget), path);

	git_buf_free(&linktarget);

	return error;
}

static int checkout_blob(
	git_repository *repo,
	git_oid *blob_oid,
	const char *path,
	unsigned int filemode,
	bool can_symlink,
	git_checkout_opts *opts)
{
	git_blob *blob;
	int error;

	if (git_blob_lookup(&blob, repo, blob_oid) < 0)
		return -1; /* Add an error message */

	if (S_ISLNK(filemode))
		error = blob_content_to_link(blob, path, can_symlink);
	else
		error = blob_content_to_file(blob, path, filemode, opts);

	git_blob_free(blob);

	return error;
}

static int checkout_diff_fn(
	void *cb_data,
	git_diff_delta *delta,
	float progress)
{
	struct checkout_diff_data *data;
	int error = -1;

	data = (struct checkout_diff_data *)cb_data;

	data->stats->processed = (unsigned int)(data->stats->total * progress);

	git_buf_truncate(data->path, data->workdir_len);
	if (git_buf_joinpath(data->path, git_buf_cstr(data->path), delta->new_file.path) < 0)
		return -1;

	switch (delta->status) {
	case GIT_DELTA_UNTRACKED:
		if (!(data->checkout_opts->checkout_strategy & GIT_CHECKOUT_REMOVE_UNTRACKED))
			return 0;

		if (!git__suffixcmp(delta->new_file.path, "/"))
			error = git_futils_rmdir_r(git_buf_cstr(data->path), GIT_DIRREMOVAL_FILES_AND_DIRS);
		else
			error = p_unlink(git_buf_cstr(data->path));
		break;

	case GIT_DELTA_MODIFIED:
		if (!(data->checkout_opts->checkout_strategy & GIT_CHECKOUT_OVERWRITE_MODIFIED))
			return 0;

		if (checkout_blob(
				data->owner,
				&delta->old_file.oid,
				git_buf_cstr(data->path),
				delta->old_file.mode,
				data->can_symlink,
				data->checkout_opts) < 0)
			goto cleanup;

		break;

	case GIT_DELTA_DELETED:
		if (!(data->checkout_opts->checkout_strategy & GIT_CHECKOUT_CREATE_MISSING))
			return 0;

		if (checkout_blob(
				data->owner,
				&delta->old_file.oid,
				git_buf_cstr(data->path),
				delta->old_file.mode,
				data->can_symlink,
				data->checkout_opts) < 0)
			goto cleanup;

		break;

	default:
		giterr_set(GITERR_INVALID, "Unexpected status (%d) for path '%s'.", delta->status, delta->new_file.path);
		goto cleanup;
	}

	error = 0;

cleanup:
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
	git_index *index = NULL;
	git_diff_list *diff = NULL;
	git_indexer_stats dummy_stats;

	git_diff_options diff_opts = {0};
	git_checkout_opts checkout_opts;

	struct checkout_diff_data data;
	git_buf workdir = GIT_BUF_INIT;

	int error;

	assert(repo);

	if ((git_repository__ensure_not_bare(repo, "checkout")) < 0)
		return GIT_EBAREREPO;

	diff_opts.flags = GIT_DIFF_INCLUDE_UNTRACKED;

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

	if ((git_repository_index(&index, repo)) < 0)
		goto cleanup;

	stats->total = git_index_entrycount(index);

	memset(&data, 0, sizeof(data));

	data.path = &workdir;
	data.workdir_len = git_buf_len(&workdir);
	data.checkout_opts = &checkout_opts;
	data.stats = stats;
	data.owner = repo;

	if ((error = retrieve_symlink_capabilities(repo, &data.can_symlink)) < 0)
		goto cleanup;

	error = git_diff_foreach(diff, &data, checkout_diff_fn, NULL, NULL);

cleanup:
	git_index_free(index);
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

