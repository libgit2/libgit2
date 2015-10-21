/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/worktree.h"

#include "common.h"
#include "repository.h"

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
