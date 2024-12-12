/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <git2.h>
#include "common.h"
#include "cmd.h"
#include "error.h"
#include "sighandler.h"
#include "progress.h"
#include "console.h"
#include "system.h"

#include "fs_path.h"
#include "futils.h"

#define COMMAND_NAME "clone"

static char *branch, *remote_path, *local_path, *depth;
static int quiet, checkout = 1, bare;
static bool local_path_exists;

static const cli_opt_spec opts[] = {
	CLI_COMMON_OPT,

	{ CLI_OPT_TYPE_SWITCH,    "quiet",       'q', &quiet,       1,
	  CLI_OPT_USAGE_DEFAULT,   NULL,         "display the type of the object" },
	{ CLI_OPT_TYPE_SWITCH,    "no-checkout", 'n', &checkout,    0,
	  CLI_OPT_USAGE_DEFAULT,   NULL,         "don't checkout HEAD" },
	{ CLI_OPT_TYPE_SWITCH,    "bare",         0,  &bare,        1,
	  CLI_OPT_USAGE_DEFAULT,   NULL,         "don't create a working directory" },
	{ CLI_OPT_TYPE_VALUE,     "branch",      'b', &branch,      0,
	  CLI_OPT_USAGE_DEFAULT,  "name",        "branch to check out" },
	{ CLI_OPT_TYPE_VALUE,     "depth",       0,   &depth,       0,
	  CLI_OPT_USAGE_DEFAULT,  "depth",       "commit depth to check out " },
	{ CLI_OPT_TYPE_LITERAL },
	{ CLI_OPT_TYPE_ARG,       "repository",   0,  &remote_path, 0,
	  CLI_OPT_USAGE_REQUIRED, "repository",  "repository path" },
	{ CLI_OPT_TYPE_ARG,       "directory",    0,  &local_path,  0,
	  CLI_OPT_USAGE_DEFAULT,  "directory",    "directory to clone into" },
	{ 0 }
};

#define CREDENTIAL_RETRY_MAX 3

struct clone_callback_data {
	cli_progress progress;
	size_t credential_retries;
	git_str password;
};

static struct clone_callback_data callback_data = { CLI_PROGRESS_INIT };

static void print_help(void)
{
	cli_opt_usage_fprint(stdout, PROGRAM_NAME, COMMAND_NAME, opts, 0);
	printf("\n");

	printf("Clone a repository into a new directory.\n");
	printf("\n");

	printf("Options:\n");

	cli_opt_help_fprint(stdout, opts);
}

static char *compute_local_path(const char *orig_path)
{
	const char *slash;
	char *local_path;

	if ((slash = strrchr(orig_path, '/')) == NULL &&
	    (slash = strrchr(orig_path, '\\')) == NULL)
		local_path = git__strdup(orig_path);
	else
		local_path = git__strdup(slash + 1);

	return local_path;
}

static int compute_depth(const char *depth)
{
	int64_t i;
	const char *endptr;

	if (!depth)
		return 0;

	if (git__strntol64(&i, depth, strlen(depth), &endptr, 10) < 0 || i < 0 || i > INT_MAX || *endptr) {
		fprintf(stderr, "fatal: depth '%s' is not valid.\n", depth);
		exit(128);
	}

	return (int)i;
}

static bool validate_local_path(const char *path)
{
	if (!git_fs_path_exists(path))
		return false;

	if (!git_fs_path_isdir(path) || !git_fs_path_is_empty_dir(path)) {
		fprintf(stderr, "fatal: destination path '%s' already exists and is not an empty directory.\n",
			path);
		exit(128);
	}

	return true;
}

static void cleanup(void)
{
	int rmdir_flags = GIT_RMDIR_REMOVE_FILES;

	cli_progress_abort(&callback_data.progress);

	if (local_path_exists)
		rmdir_flags |= GIT_RMDIR_SKIP_ROOT;

	if (!git_fs_path_isdir(local_path))
		return;

	git_futils_rmdir_r(local_path, NULL, rmdir_flags);
}

static void interrupt_cleanup(void)
{
	cleanup();
	exit(130);
}

static int find_keys(git_str *pub, git_str *priv)
{
	git_str path = GIT_STR_INIT;
	static const char *key_paths[6] = {
		"id_dsa", "id_ecdsa", "id_ecdsa_sk",
		"id_ed25519", "id_ed25519_sk", "id_rsa"
	};
	size_t i, path_len;
	int error = -1;

	if (git_system_homedir(&path) < 0)
		goto done;

	path_len = git_str_len(&path);

	for (i = 0; i < ARRAY_SIZE(key_paths); i++) {
		git_str_truncate(&path, path_len);

		if (git_str_puts(&path, "/.ssh/") < 0 ||
		    git_str_puts(&path, key_paths[i]) < 0)
			goto done;

		if (git_fs_path_exists(path.ptr)) {
			if (git_str_puts(priv, path.ptr) < 0 ||
			    git_str_puts(pub, path.ptr) < 0 ||
			    git_str_puts(pub, ".pub") < 0)
				goto done;

			error = 0;
			goto done;
		}
	}

	error = GIT_ENOTFOUND;

done:
	git_str_dispose(&path);
	return error;
}

static int clone_credentials(
	git_credential **out,
	const char *url,
	const char *username_from_url,
	unsigned int allowed_types,
	void *payload)
{
	struct clone_callback_data *data = (struct clone_callback_data *)payload;
	git_str pubkey = GIT_STR_INIT, privkey = GIT_STR_INIT,
	        prompt = GIT_STR_INIT;
	int error = GIT_PASSTHROUGH;

	GIT_UNUSED(url);

	if (++data->credential_retries > CREDENTIAL_RETRY_MAX) {
		cli_error("authentication failed");
		error = GIT_EUSER;
		goto done;
	}

	if ((allowed_types & GIT_CREDENTIAL_SSH_KEY)) {
		if ((error = find_keys(&pubkey, &privkey)) < 0) {
			if (error == GIT_ENOTFOUND) {
				cli_error("could not find ssh keys for authentication");
				error = GIT_EUSER;
			}

			goto done;
		}

		if ((error = git_str_printf(&prompt, "Enter passphrase for key '%s': ", pubkey.ptr)) < 0 ||
		    (error = cli_console_getpass(&data->password, prompt.ptr)) < 0)
			goto done;

		error = git_credential_ssh_key_new(out,
			username_from_url,
			pubkey.ptr,
			privkey.ptr,
			data->password.ptr);
	}

done:
	git_str_zero(&data->password);
	git_str_dispose(&prompt);
	git_str_dispose(&pubkey);
	git_str_dispose(&privkey);
	return error;
}

static int clone_progress_sideband(const char *str, int len, void *payload)
{
	struct clone_callback_data *data = (struct clone_callback_data *)payload;
	return cli_progress_fetch_sideband(str, len, &data->progress);
}

static int clone_progress_transfer(
	const git_indexer_progress *stats,
	void *payload)
{
	struct clone_callback_data *data = (struct clone_callback_data *)payload;
	return cli_progress_fetch_transfer(stats, &data->progress);
}

static void clone_progress_checkout(
	const char *path,
	size_t completed_steps,
	size_t total_steps,
	void *payload)
{
	struct clone_callback_data *data = (struct clone_callback_data *)payload;
	return cli_progress_checkout(path, completed_steps, total_steps, &data->progress);
}

int cmd_clone(int argc, char **argv)
{
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *repo = NULL;
	cli_opt invalid_opt;
	char *computed_path = NULL;
	int ret = 0;

	if (cli_opt_parse(&invalid_opt, opts, argv + 1, argc - 1, CLI_OPT_PARSE_GNU))
		return cli_opt_usage_error(COMMAND_NAME, opts, &invalid_opt);

	if (cli_opt__show_help) {
		print_help();
		return 0;
	}

	if (!remote_path) {
		ret = cli_error_usage("you must specify a repository to clone");
		goto done;
	}

	clone_opts.bare = !!bare;
	clone_opts.checkout_branch = branch;
	clone_opts.fetch_opts.depth = compute_depth(depth);

	if (!checkout)
		clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_NONE;

	if (!local_path)
		local_path = computed_path = compute_local_path(remote_path);

	local_path_exists = validate_local_path(local_path);

	cli_sighandler_set_interrupt(interrupt_cleanup);

	if (!local_path_exists &&
	    git_futils_mkdir(local_path, 0777, 0) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if (!quiet) {
		clone_opts.fetch_opts.callbacks.credentials = clone_credentials;
		clone_opts.fetch_opts.callbacks.sideband_progress = clone_progress_sideband;
		clone_opts.fetch_opts.callbacks.transfer_progress = clone_progress_transfer;
		clone_opts.fetch_opts.callbacks.payload = &callback_data;

		clone_opts.checkout_opts.progress_cb = clone_progress_checkout;
		clone_opts.checkout_opts.progress_payload = &callback_data;

		printf("Cloning into '%s'...\n", local_path);
	}

	if ((ret = git_clone(&repo, remote_path, local_path, &clone_opts)) < 0) {
		cleanup();

		if (ret != GIT_EUSER)
			ret = cli_error_git();

		goto done;
	}

	cli_progress_finish(&callback_data.progress);

done:
	cli_progress_dispose(&callback_data.progress);
	git_str_zero(&callback_data.password);
	git__free(computed_path);
	git_repository_free(repo);
	return ret;
}
