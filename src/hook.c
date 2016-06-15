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

#define GIT_HOOKS_DIR "hooks/"

const char * const hooks[] = {
	"applypatch-msg",
	"commit-msg",
	"post-update",
	"pre-applypatch",
	"pre-commit",
	"pre-push",
	"pre-rebase",
	"prepare-commit-msg",
	"update",
};

#define MAX_HOOK_LEN 18+1
#define HOOK_FILE_MODE 0776

static int build_hook_path(char **hook_path, git_repository *repo, const char *hook_name)
{
	int hook_dir_len;

	assert(hook_path);

	hook_dir_len = strlen(repo->path_repository) + strlen(GIT_HOOKS_DIR);
	*hook_path = git__malloc(hook_dir_len + MAX_HOOK_LEN);
	GITERR_CHECK_ALLOC(*hook_path);

	strcpy(*hook_path, repo->path_repository);
	strcat(*hook_path, GIT_HOOKS_DIR);
	strcat(*hook_path, hook_name);

	return 0;
}

static int check_hook(git_repository *repo, const char *hook_name)
{
	int err;
	char *hook_path;
	struct stat hook_stat;

	assert(repo && hook_name);

	err = build_hook_path(&hook_path, repo, hook_name);
	if (err) goto error;

	/* Skip missing hooks */
	err = p_stat(hook_path, &hook_stat);
	if (err) goto error;

	/* Check exec bits */
	if ((hook_stat.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0) goto error;

	return 0;

error:
	git__free(hook_path);
	return -1;
}

int git_hook_enumerate(
	git_repository *repo,
	git_hook_foreach_cb callback,
	void *payload)
{
	assert(repo && callback);

	for (size_t hook_id = 0; hook_id <= sizeof(*hooks); hook_id++) {
		const char *hook_name = hooks[hook_id];
		int err = 0;

		err = check_hook(repo, hook_name);
		if (err) continue;

		callback(hook_name, payload);
	}

	return 0;
}

int git_hook_load(git_buf *out, git_repository *repo, const char *hook_name)
{
	char *hook_path;
	int err;

	assert(out && repo && hook_name);

	err = build_hook_path(&hook_path, repo, hook_name);
	if (err) {
		giterr_set(GITERR_REPOSITORY, "hook \"%s\" does not exist");
		return -1;
	}

	return git_futils_readbuffer(out, hook_path);
}

int git_hook_save(git_buf *contents, git_repository *repo, const char *hook_name)
{
	char *hook_path;
	int err;

	assert(contents && repo && hook_name);

	err = build_hook_path(&hook_path, repo, hook_name);
	if (err) {
		giterr_set(GITERR_REPOSITORY, "hook \"%s\" does not exist");
		return -1;
	}

	return git_futils_writebuffer(contents, hook_path, O_CREAT | O_TRUNC | O_WRONLY, HOOK_FILE_MODE);
}
