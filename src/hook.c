/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "posix.h"
#include "repository.h"

#include "git2/hook.h"
#include "git2/sys/hook.h"


const char * const githooks[] = {
	"applypatch-msg",
	"pre-applypatch",
	"post-applypatch",
	"pre-commit",
	"prepare-commit-msg",
	"commit-msg",
	"post-commit",
	"pre-rebase",
	"post-checkout",
	"post-merge",
	"pre-push",
	"pre-receive",
	"update",
	"post-receive",
	"post-update",
	"push-to-checkout",
	"pre-auto-gc",
	"post-rewrite",
};

int git_hook_dir(git_buf *out_dir, git_repository *repo)
{
	int err;
	git_buf cfg_path = GIT_BUF_INIT;
	git_config *cfg;

	assert(out_dir && repo);

	/* We need to check for an override in the repo config */
	err = git_repository_config__weakptr(&cfg, repo);
	if (err != 0)
		return err;

	err = git_config_get_path(&cfg_path, cfg, "core.hooksPath");
	if (err == GIT_ENOTFOUND) {
		git_error_clear();
		if ((err = git_repository_item_path(out_dir, repo, GIT_REPOSITORY_ITEM_HOOKS)) < 0)
			return err;
	} else if (err == GIT_OK) {
		git_buf_joinpath(out_dir, git_repository_commondir(repo), cfg_path.ptr);
		git_path_resolve_relative(out_dir, 0);
		git_buf_dispose(&cfg_path);
	} else {
		return err;
	}

	return 0;
}

static int build_hook_path(git_buf *out_path, git_repository *repo, const char *hook_name)
{
	int err;
	git_buf hook_path = GIT_BUF_INIT;

	assert(hook_name);

	err = git_hook_dir(&hook_path, repo);
	if (err != 0)
		return -1;

	err = git_buf_joinpath(&hook_path, hook_path.ptr, hook_name);
	if (err != 0)
		return -1;

	*out_path = hook_path;

	return 0;
}

static int check_hook_path(const git_buf *hook_path)
{
	int err;
	struct stat hook_stat;

	assert(hook_path);

	/* Skip missing hooks */
	err = p_stat(git_buf_cstr(hook_path), &hook_stat);
	if (err == ENOENT) {
		git_error_set(GIT_ERROR_HOOK, "hook %s wasn't found", git_buf_cstr(hook_path));
		return GIT_ENOTFOUND;
	} else if (err) {
		git_error_set(GIT_ERROR_OS, "failed to stat %s", git_buf_cstr(hook_path));
		return -1;
	}

#ifndef GIT_WIN32
	/* Check exec bits */
	if ((hook_stat.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0) {
		git_error_set(GIT_ERROR_HOOK, "can't exec hook %s", git_buf_cstr(hook_path));
		return -1;
	}
#endif

	return 0;
}

int git_hook_foreach(
	git_repository *repo,
	git_hook_foreach_cb callback,
	void *payload)
{
	size_t hook_id = 0;

	assert(repo && callback);

	for (hook_id = 0; hook_id < ARRAY_SIZE(githooks); hook_id++) {
		const char *hook_name = githooks[hook_id];
		int err = 0;
		git_buf hook_path = GIT_BUF_INIT;

		err = build_hook_path(&hook_path, repo, hook_name);
		if (err != 0)
			continue;

		err = check_hook_path(&hook_path);
		git_buf_dispose(&hook_path);

		if (err != 0)
			continue;

		err = callback(hook_name, payload);
		if (err != 0)
			break;
	}

	return 0;
}
