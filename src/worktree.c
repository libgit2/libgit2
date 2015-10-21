/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/worktree.h"

#include "common.h"
#include "repository.h"
#include "worktree.h"

static bool is_worktree_dir(git_buf *dir)
{
	return git_path_contains_file(dir, "commondir")
		&& git_path_contains_file(dir, "gitdir")
		&& git_path_contains_file(dir, "HEAD");
}

int git_worktree_list(git_strarray *wts, git_repository *repo)
{
	git_vector worktrees = GIT_VECTOR_INIT;
	git_buf path = GIT_BUF_INIT;
	char *worktree;
	unsigned i, len;
	int error;

	assert(wts && repo);

	wts->count = 0;
	wts->strings = NULL;

	if ((error = git_buf_printf(&path, "%s/worktrees/", repo->commondir)) < 0)
		goto exit;
	if (!git_path_exists(path.ptr) || git_path_is_empty_dir(path.ptr))
		goto exit;
	if ((error = git_path_dirload(&worktrees, path.ptr, path.size, 0x0)) < 0)
		goto exit;

	len = path.size;

	git_vector_foreach(&worktrees, i, worktree) {
		git_buf_truncate(&path, len);
		git_buf_puts(&path, worktree);

		if (!is_worktree_dir(&path)) {
			git_vector_remove(&worktrees, i);
			git__free(worktree);
		}
	}

	wts->strings = (char **)git_vector_detach(&wts->count, NULL, &worktrees);

exit:
	git_buf_free(&path);

	return error;
}

static char *read_link(const char *base, const char *file)
{
	git_buf path = GIT_BUF_INIT, buf = GIT_BUF_INIT;

	assert(base && file);

	if (git_buf_joinpath(&path, base, file) < 0)
		goto err;
	if (git_futils_readbuffer(&buf, path.ptr) < 0)
		goto err;
	git_buf_free(&path);

	git_buf_rtrim(&buf);

	if (!git_path_is_relative(buf.ptr))
		return git_buf_detach(&buf);

	if (git_buf_sets(&path, base) < 0)
		goto err;
	if (git_path_apply_relative(&path, buf.ptr) < 0)
		goto err;
	git_buf_free(&buf);

	return git_buf_detach(&path);

err:
	git_buf_free(&buf);
	git_buf_free(&path);

	return NULL;
}

int git_worktree_lookup(git_worktree **out, git_repository *repo, const char *name)
{
	git_buf path = GIT_BUF_INIT;
	git_worktree *wt = NULL;
	int error;

	assert(repo && name);

	*out = NULL;

	if ((error = git_buf_printf(&path, "%s/worktrees/%s", repo->commondir, name)) < 0)
		goto out;

	if (!is_worktree_dir(&path)) {
		error = -1;
		goto out;
	}

	if ((wt = git__malloc(sizeof(struct git_repository))) == NULL) {
		error = -1;
		goto out;
	}

	if ((wt->name = git__strdup(name)) == NULL
	    || (wt->commondir_path = read_link(path.ptr, "commondir")) == NULL
	    || (wt->gitlink_path = read_link(path.ptr, "gitdir")) == NULL
	    || (wt->parent_path = git__strdup(git_repository_path(repo))) == NULL) {
		error = -1;
		goto out;
	}
	wt->gitdir_path = git_buf_detach(&path);

	(*out) = wt;

out:
	git_buf_free(&path);

	if (error)
		git_worktree_free(wt);

	return error;
}

void git_worktree_free(git_worktree *wt)
{
	if (!wt)
		return;

	git__free(wt->commondir_path);
	git__free(wt->gitlink_path);
	git__free(wt->gitdir_path);
	git__free(wt->parent_path);
	git__free(wt->name);
	git__free(wt);
}
