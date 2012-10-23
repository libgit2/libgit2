/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "repository.h"
#include "buffer.h"
#include "merge.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "git2/reset.h"

int git_merge_inprogress(int *out, git_repository *repo)
{
	int error = 0;
	git_buf merge_head_path = GIT_BUF_INIT;

	assert(repo);

	if ((error = git_buf_joinpath(&merge_head_path, repo->path_repository, MERGE_HEAD_FILE)) == 0)
		*out = git_path_exists(merge_head_path.ptr);

	git_buf_free(&merge_head_path);
	return error;
}

int git_merge__cleanup(git_repository *repo)
{
	int error = 0;
	git_buf merge_head_path = GIT_BUF_INIT,
		merge_mode_path = GIT_BUF_INIT,
		merge_msg_path = GIT_BUF_INIT;

	assert(repo);

	if ((error = git_buf_joinpath(&merge_head_path, repo->path_repository, MERGE_HEAD_FILE)) < 0)
		goto cleanup;

	if ((error = git_buf_joinpath(&merge_mode_path, repo->path_repository, MERGE_MODE_FILE)) < 0)
		goto cleanup;

	if ((error = git_buf_joinpath(&merge_msg_path, repo->path_repository, MERGE_MSG_FILE)) < 0)
		goto cleanup;

	if (git_path_exists(merge_head_path.ptr))
	{
		if ((error = p_unlink(merge_head_path.ptr)) < 0)
			goto cleanup;
	}

	if (git_path_exists(merge_mode_path.ptr))
		(void)p_unlink(merge_mode_path.ptr);

	if (git_path_exists(merge_msg_path.ptr))
		(void)p_unlink(merge_msg_path.ptr);

cleanup:
	git_buf_free(&merge_msg_path);
	git_buf_free(&merge_mode_path);
	git_buf_free(&merge_head_path);

	return error;
}

