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
#include "git2/commit.h"
#include "git2/blob.h"
#include "git2/config.h"

#include "common.h"
#include "refs.h"
#include "buffer.h"
#include "repository.h"
#include "filter.h"
#include "blob.h"

GIT_BEGIN_DECL


typedef struct tree_walk_data
{
	git_indexer_stats *stats;
	git_checkout_opts *opts;
	git_repository *repo;
	git_odb *odb;
	bool no_symlinks;
} tree_walk_data;


static int blob_contents_to_link(tree_walk_data *data, git_buf *fnbuf,
											const git_oid *id)
{
	int retcode = GIT_ERROR;
	git_blob *blob;

	/* Get the link target */
	if (!(retcode = git_blob_lookup(&blob, data->repo, id))) {
		git_buf linktarget = GIT_BUF_INIT;
		if (!(retcode = git_blob__getbuf(&linktarget, blob))) {
			/* Create the link */
			const char *new = git_buf_cstr(&linktarget),
						  *old = git_buf_cstr(fnbuf);
			retcode = data->no_symlinks
				? git_futils_fake_symlink(new, old)
				: p_symlink(new, old);
		}
		git_buf_free(&linktarget);
		git_blob_free(blob);
	}

	return retcode;
}


static int blob_contents_to_file(git_repository *repo, git_buf *fnbuf,
											const git_tree_entry *entry, tree_walk_data *data)
{
	int retcode = GIT_ERROR;
	int fd = -1;
	git_buf contents = GIT_BUF_INIT;
	const git_oid *id = git_tree_entry_id(entry);
	int file_mode = data->opts->file_mode;

	/* Deal with pre-existing files */
	if (git_path_exists(git_buf_cstr(fnbuf)) &&
		 data->opts->existing_file_action == GIT_CHECKOUT_SKIP_EXISTING)
		return 0;

	/* Allow disabling of filters */
	if (data->opts->disable_filters) {
		git_blob *blob;
		if (!(retcode = git_blob_lookup(&blob, repo, id))) {
			retcode = git_blob__getbuf(&contents, blob);
			git_blob_free(blob);
		}
	} else {
		retcode = git_filter_blob_contents(&contents, repo, id, git_buf_cstr(fnbuf));
	}
	if (retcode < 0) goto bctf_cleanup;

	/* Allow overriding of file mode */
	if (!file_mode)
		file_mode = git_tree_entry_filemode(entry);

	if ((retcode = git_futils_mkpath2file(git_buf_cstr(fnbuf), data->opts->dir_mode)) < 0)
		goto bctf_cleanup;

	fd = p_open(git_buf_cstr(fnbuf), data->opts->file_open_flags, file_mode);
	if (fd < 0) goto bctf_cleanup;

	if (!p_write(fd, git_buf_cstr(&contents), git_buf_len(&contents)))
		retcode = 0;
	else
		retcode = GIT_ERROR;
	p_close(fd);

bctf_cleanup:
	git_buf_free(&contents);
	return retcode;
}

static int checkout_walker(const char *path, const git_tree_entry *entry, void *payload)
{
	int retcode = 0;
	tree_walk_data *data = (tree_walk_data*)payload;
	int attr = git_tree_entry_filemode(entry);
	git_buf fnbuf = GIT_BUF_INIT;
	git_buf_join_n(&fnbuf, '/', 3,
						git_repository_workdir(data->repo),
						path,
						git_tree_entry_name(entry));

	switch(git_tree_entry_type(entry))
	{
	case GIT_OBJ_TREE:
		/* Nothing to do; the blob handling creates necessary directories. */
		break;

	case GIT_OBJ_COMMIT:
		/* Submodule */
		git_futils_mkpath2file(git_buf_cstr(&fnbuf), data->opts->dir_mode);
		retcode = p_mkdir(git_buf_cstr(&fnbuf), data->opts->dir_mode);
		break;

	case GIT_OBJ_BLOB:
		if (S_ISLNK(attr)) {
			retcode = blob_contents_to_link(data, &fnbuf,
													  git_tree_entry_id(entry));
		} else {
			retcode = blob_contents_to_file(data->repo, &fnbuf, entry, data);
		}
		break;

	default:
		retcode = -1;
		break;
	}

	git_buf_free(&fnbuf);
	data->stats->processed++;
	return retcode;
}


int git_checkout_head(git_repository *repo, git_checkout_opts *opts, git_indexer_stats *stats)
{
	int retcode = GIT_ERROR;
	git_indexer_stats dummy_stats;
	git_checkout_opts default_opts = {0};
	git_tree *tree;
	tree_walk_data payload;
	git_config *cfg;

	assert(repo);
	if (!opts) opts = &default_opts;
	if (!stats) stats = &dummy_stats;

	/* Default options */
	if (!opts->existing_file_action)
		opts->existing_file_action = GIT_CHECKOUT_OVERWRITE_EXISTING;
	/* opts->disable_filters is false by default */
	if (!opts->dir_mode) opts->dir_mode = GIT_DIR_MODE;
	if (!opts->file_open_flags)
		opts->file_open_flags = O_CREAT | O_TRUNC | O_WRONLY;

	if (git_repository_is_bare(repo)) {
		giterr_set(GITERR_INVALID, "Checkout is not allowed for bare repositories");
		return GIT_ERROR;
	}

	memset(&payload, 0, sizeof(payload));

	/* Determine if symlinks should be handled */
	if (!git_repository_config__weakptr(&cfg, repo)) {
		int temp = true;
		if (!git_config_get_bool(&temp, cfg, "core.symlinks")) {
			payload.no_symlinks = !temp;
		}
	}

	stats->total = stats->processed = 0;
	payload.stats = stats;
	payload.opts = opts;
	payload.repo = repo;
	if (git_repository_odb(&payload.odb, repo) < 0) return GIT_ERROR;

	if (!git_repository_head_tree(&tree, repo)) {
		git_index *idx;
		if (!(retcode = git_repository_index(&idx, repo))) {
			if (!(retcode = git_index_read_tree(idx, tree, stats))) {
				git_index_write(idx);
				retcode = git_tree_walk(tree, checkout_walker, GIT_TREEWALK_POST, &payload);
			}
			git_index_free(idx);
		}
		git_tree_free(tree);
	}

	git_odb_free(payload.odb);
	return retcode;
}


int git_checkout_reference(git_reference *ref,
									git_checkout_opts *opts,
									git_indexer_stats *stats)
{
	git_repository *repo= git_reference_owner(ref);
	git_reference *head = NULL;
	int retcode = GIT_ERROR;

	if ((retcode = git_reference_create_symbolic(&head, repo, GIT_HEAD_FILE,
																git_reference_name(ref), true)) < 0)
		return retcode;

	retcode = git_checkout_head(git_reference_owner(ref), opts, stats);

	git_reference_free(head);
	return retcode;
}


GIT_END_DECL
