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

int git_hook_dir(git_buf *out_dir, git_repository *repo)
{
	int err;
	git_buf tmp_path = GIT_BUF_INIT;
	git_buf cfg_path = GIT_BUF_INIT;
	git_config *cfg;

	assert(out_dir && repo);

	/* We need to check for an override in the repo config */
	err = git_repository_config__weakptr(&cfg, repo);
	if (err != 0)
		return err;

	err = git_config_get_path(&cfg_path, cfg, "core.hooksPath");
	if (err == GIT_ENOTFOUND) {
		git_buf_puts(&tmp_path, repo->path_repository);
		git_buf_joinpath(&tmp_path, tmp_path.ptr, GIT_HOOKS_DIR);
	} else if (err == GIT_OK) {
		git_buf_joinpath(&tmp_path, repo->path_repository, cfg_path.ptr);
		git_path_resolve_relative(&tmp_path, 0);
	} else {
		/* XXX: error reporting */
		return NULL;
	}

	*out_dir = tmp_path;
	return 0;
}

static int build_hook_path(char **out_path, git_repository *repo, const char *hook_name)
{
	int err;
	git_buf hook_path = GIT_BUF_INIT;

	assert(hook_name);

	err = git_hook_dir(&hook_path, repo);
	if (err != 0)
		return -1;

	git_buf_joinpath(&hook_path, hook_path.ptr, hook_name);

	*out_path = git_buf_detach(&hook_path);

	return 0;
}

static int check_hook_path(const char *hook_path)
{
	int err;
	struct stat hook_stat;

	assert(hook_path);

	/* Skip missing hooks */
	err = p_stat(hook_path, &hook_stat);
	if (err)
		return -1;

#ifndef GIT_WIN32
	/* Check exec bits */
	if ((hook_stat.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0) return -1;
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
		char *hook_path = NULL;

		err = build_hook_path(&hook_path, repo, hook_name);
		if (err != 0)
			continue;

		err = check_hook_path(hook_path);
		git__free(hook_path);

		if (err != 0)
			continue;

		callback(hook_name, payload);
	}

	return 0;
}

int git_hook_register_callback(git_repository *repo,
							   git_hook_execution_cb executor,
							   git_hook_destructor_cb destructor,
							   void *payload)
{
	assert(repo && executor);

	/* Unset our payload-memory-management if needed */
	if (repo->hook_payload != NULL && repo->hook_payload_free != NULL) {
		repo->hook_payload_free(repo->hook_payload);
		repo->hook_payload_free = NULL;
	}

	repo->hook_executor = executor;
	repo->hook_payload = payload;
	repo->hook_payload_free = destructor;

	return 0;
}

static int hook_execute_va(git_buf *io, git_repository *repo, const char *name, va_list args)
{
	int err = 0;
	char *arg;
	git_vector arg_vector = GIT_VECTOR_INIT;
	git_hook_env env;

	assert(repo && name);

	err = build_hook_path(&env.path, repo, name);
	if (err != 0)
		goto cleanup;

	err = check_hook_path(env.path);
	if (err != 0)
		goto cleanup;

	while ((arg = va_arg(args, char *))) {
		if (arg == NULL)
			break;

		git_vector_insert(&arg_vector, arg);
	}

	va_end(args);

	env.io = io;

	env.args.strings = (char **)git_vector_detach(&env.args.count, NULL, &arg_vector);

	err = repo->hook_executor(env, repo->hook_payload);

cleanup:
	git__free(env.path);
	git__free(env.args.strings);
	return err;
}

int git_hook_execute(git_repository *repo, const char *hook_name, ...)
{
	int err = 0;
	va_list hook_args;

	va_start(hook_args, hook_name);
	err = hook_execute_va(NULL, repo, hook_name, hook_args);
	va_end(hook_args);

	return err;
}

int git_hook_execute_io(git_buf *io, git_repository *repo, const char *hook_name, ...)
{
	int err = 0;
	va_list hook_args;

	va_start(hook_args, hook_name);
	err = hook_execute_va(io, repo, hook_name, hook_args);
	va_end(hook_args);

	return err;
}
