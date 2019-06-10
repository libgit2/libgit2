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
	if (err && errno == ENOENT) {
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

int git_hook_register_callback(git_repository *repo,
	git_hook_execution_cb executor,
	git_hook_destructor_cb destructor,
	void *payload)
{
	assert(repo && executor);

	/* Unset our payload-memory-management if needed */
	if (repo->hook_payload_free != NULL) {
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
	git_buf hook_path = GIT_BUF_INIT;

	assert(repo && name);

	memset(&env, '\0', sizeof(env));

	err = build_hook_path(&hook_path, repo, name);
	if (err != 0)
		goto cleanup;

	err = check_hook_path(&hook_path);
	if (err == GIT_ENOTFOUND) {
		/* Ignore missing hook */
		git_error_clear();
		err = 0;
		goto cleanup;
	} else if (err) {
		/* Report problem */
		goto cleanup;
	}

	while ((arg = va_arg(args, char *))) {
		if (arg == NULL)
			break;

		if (git_vector_insert(&arg_vector, arg) != 0) {
			git_error_set_oom();
			return -1;
		}
	}

	env.path = hook_path.ptr;
	env.io = io;
	env.args.strings = (char **)git_vector_detach(&env.args.count, NULL, &arg_vector);

	err = repo->hook_executor(&env, repo->hook_payload);
	if (err < 0 && !git_error_last()) {
		git_error_set(GIT_ERROR_HOOK, "hook \"%s\" reported failure", name);
		goto cleanup;
	}

cleanup:
	git__free(env.args.strings);
	git_buf_dispose(&hook_path);
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
