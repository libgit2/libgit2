/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "repository.h"
#include "git2/repository.h"
#include "git2/gitup_repository.h"

#include "futils.h"
#include "index.h"
#include "config.h"
#include "odb.h"
#include "refdb.h"
#include "diff_driver.h"

int gitup_repository_update_gitlink(
	git_repository *repo,
	int use_relative_path)
{
	int error;
	const char *workdir = git_repository_workdir(repo);
	int update_gitlink = 1;

	if ((error = git_futils_mkdir(workdir, 0777, GIT_MKDIR_PATH | GIT_MKDIR_VERIFY_DIR)) < 0)
		return error;

	if ((error = git_repository_set_workdir(repo, workdir, update_gitlink)) < 0 )
		return error;

	return 0;
}

GIT_EXTERN(int) gitup_repository_local_config_path(git_buf *out, git_repository *repo)
{
    return git_repository_item_path(out, repo, GIT_REPOSITORY_ITEM_CONFIG);
}
