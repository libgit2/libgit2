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

static int check_hook_path(const char *hook_path)
{
	int err;
	struct stat hook_stat;

	assert(hook_path);

	/* Skip missing hooks */
	err = p_stat(hook_path, &hook_stat);
	if (err) return -1;

	/* Check exec bits */
	if ((hook_stat.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0) return -1;

	return 0;
}

int git_hook_enumerate(
	git_repository *repo,
	git_hook_foreach_cb callback,
	void *payload)
{
	assert(repo && callback);

	for (size_t hook_id = 0; hook_id < sizeof(githooks) / sizeof(char *); hook_id++) {
		const char *hook_name = githooks[hook_id];
		int err = 0;
		char *hook_path = NULL;

		err = build_hook_path(&hook_path, repo, hook_name);
		if (err != 0) continue;

		err = check_hook_path(hook_path);
		git__free(hook_path);

		if (err != 0) continue;

		callback(hook_name, payload);
	}

	return 0;
}

int git_hook_register_callback(git_repository *repo, git_hook_execution_cb callback, void *payload)
{
	assert(repo && callback);

	repo->hook_executor = callback;
	repo->hook_payload = payload;

	return 0;
}

int git_hook_exec_va(git_repository *repo, const char *name, va_list args)
{
	int err = 0;
	char *arg;
	git_vector arg_vector = GIT_VECTOR_INIT;
	git_hook_env env;

	assert(repo && name);

	err = build_hook_path(&env.path, repo, name);
	if (err != 0) goto cleanup;

	err = check_hook_path(env.path);
	if (err != 0) goto cleanup;

	while ((arg = va_arg(args, char *)))
	{
		if (arg == NULL) break;
		git_vector_insert(&arg_vector, arg);
	}

	va_end(args);

	env.args.strings = (char **)git_vector_detach(&env.args.count, NULL, &arg_vector);

	err = repo->hook_executor(env, repo->hook_payload);

cleanup:
	git__free(env.path);
	return err;
}

int git_hook_execute(git_repository *repo, const char *hook_name, ...)
{
	int err = 0;
	va_list hook_args;

	va_start(hook_args, hook_name);
	err = git_hook_exec_va(repo, hook_name, hook_args);
	va_end(hook_args);

	return err;
}