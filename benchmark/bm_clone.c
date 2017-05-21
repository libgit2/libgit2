/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include "git2.h"
#include "git2/sys/repository.h"
#include "common.h"
#include "bench_util.h"
#include "buffer.h"
#include "fileops.h"
#include "opt.h"
#include "run.h"
#include "operation.h"
#include "benchmark.h"
#include "shell.h"


typedef struct gitbench_benchmark_clone {
	gitbench_benchmark base;

	char *repo_path;
	char *username;
	char *password;

	git_clone_local_t local;
	bool bare;

} gitbench_benchmark_clone;

enum clone_operation_t {
	CLONE_OPERATION_SETUP = 0,
	CLONE_OPERATION_CLONE,
	CLONE_OPERATION_CLEANUP
};

static gitbench_operation_spec clone_operations[] = {
	{ CLONE_OPERATION_SETUP,   "setup" },
	{ CLONE_OPERATION_CLONE, "clone" },
	{ CLONE_OPERATION_CLEANUP, "close" },
};
#define CLONE_OPERATIONS_COUNT (sizeof(clone_operations) / sizeof(clone_operations[0]))

static const gitbench_opt_spec clone_cmdline_opts[] = {
	{ GITBENCH_OPT_SWITCH, "help",           0, NULL,      "display help",   GITBENCH_OPT_USAGE_HIDDEN },
	{ GITBENCH_OPT_ARG,    "repository",     0, NULL,      "the repository", GITBENCH_OPT_USAGE_REQUIRED },
	{ GITBENCH_OPT_SWITCH, "local",          0, "local",        "local" },
	{ GITBENCH_OPT_SWITCH, "no-local",       0, "no-local",     "no-local" },
	{ GITBENCH_OPT_SWITCH, "no-hardlinks",   0, "no-hardlinks", "no-hardlinks" },
	{ GITBENCH_OPT_VALUE,  "username",     'u', "username", "username", GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_VALUE,  "password",     'p', "password", "password", GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ 0 }
};


/**
 * Supply credentials for the call to git_clone().
 * We use the optional command line args or the
 * environment variables.
 *
 * I don't like either of these methods, but I
 * don't want to hook up a credential helper right
 * now (and which may still prompt the user).
 *
 * This is only used by the libgit2 code; we don't
 * control what git.exe will do -- so to have a
 * fully automated test, you'll need to address
 * that separately.
 */
static int cred_cb(
	git_cred **cred,
	const char *url,
	const char *username_from_url,
	unsigned int allowed_types,
	void *payload)
{
	gitbench_benchmark_clone *benchmark = (gitbench_benchmark_clone *)payload;
	const char *user;
	const char *pass;

	GIT_UNUSED(url);
	GIT_UNUSED(allowed_types);

	if (username_from_url)
		user = username_from_url;
	else if (benchmark->username)
		user = benchmark->username;
	else
		user = getenv("BENCHMARK_USERNAME");

	if (benchmark->password)
		pass = benchmark->password;
	else
		pass = getenv("BENCHMARK_PASSWORD");

	return git_cred_userpass_plaintext_new(cred, user, pass);
}


static int _do_clone(
	gitbench_benchmark_clone *benchmark,
	const char *wd)
{
	git_repository *repo = NULL;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_remote_callbacks remote_callbacks = GIT_REMOTE_CALLBACKS_INIT;
	int error;

	checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	remote_callbacks.credentials = cred_cb;
	remote_callbacks.payload = benchmark;

	clone_opts.checkout_opts = checkout_opts;
	clone_opts.remote_callbacks = remote_callbacks;
	clone_opts.bare = benchmark->bare;
	clone_opts.local = benchmark->local;
	error = git_clone(&repo, benchmark->repo_path, wd, &clone_opts);

	git_repository_free(repo);
	return error;
}

static int _do_clone_using_git_exe(
	gitbench_benchmark_clone *benchmark,
	const char *wd)
{
	const char * argv[10] = {0};
	int k = 0;

	argv[k++] = BM_GIT_EXE;
	argv[k++] = "clone";
	argv[k++] = "--quiet";

	if (benchmark->bare)
		argv[k++] = "--bare";

	if (benchmark->local == GIT_CLONE_LOCAL)
		argv[k++] = "--local";
	else if (benchmark->local == GIT_CLONE_NO_LOCAL)
		argv[k++] = "--no-local";
	else if (benchmark->local == GIT_CLONE_LOCAL_NO_LINKS)
		argv[k++] = "--no-hardlinks";

	argv[k++] = benchmark->repo_path;
	argv[k++] = wd;
	argv[k++] = 0;

	return gitbench_shell(argv, NULL, NULL);
}

static int _time_clone(
	gitbench_benchmark_clone *benchmark,
	gitbench_run *run,
	const char *wd)
{
	int error;

	gitbench_run_start_operation(run, CLONE_OPERATION_CLONE);
	if (run->use_git_exe)
		error = _do_clone_using_git_exe(benchmark, wd);
	else
		error = _do_clone(benchmark, wd);
	gitbench_run_finish_operation(run);

	return error;
}


static int _do_setup(
	git_buf *wd_path,
	gitbench_benchmark_clone *benchmark,
	gitbench_run *run)
{
	int error;

	GIT_UNUSED(benchmark);

	if ((error = git_buf_joinpath(wd_path, run->tempdir, "wd")) < 0)
		return error;
	if ((error = git_futils_mkdir(wd_path->ptr, NULL, 0700, GIT_MKDIR_VERIFY_DIR)) < 0)
		return error;

	return error;
}

static int _time_setup(
	git_buf *wd_path,
	gitbench_benchmark_clone *benchmark,
	gitbench_run *run)
{
	int error;

	gitbench_run_start_operation(run, CLONE_OPERATION_SETUP);
	error = _do_setup(wd_path, benchmark, run);
	gitbench_run_finish_operation(run);

	return error;
}


static int clone_run(gitbench_benchmark *b, gitbench_run *run)
{
	gitbench_benchmark_clone *benchmark = (gitbench_benchmark_clone *)b;
	git_buf wd_path = GIT_BUF_INIT;
	int error;

	if ((error = _time_setup(&wd_path, benchmark, run)) < 0)
		goto done;
	
	error = _time_clone(benchmark, run, wd_path.ptr);

done:
	git_buf_free(&wd_path);
	return error;
}

static void clone_free(gitbench_benchmark *b)
{
	gitbench_benchmark_clone *benchmark = (gitbench_benchmark_clone *)b;

	if (!b)
		return;

	git__free(benchmark->repo_path);
	git__free(benchmark->username);
	git__free(benchmark->password);
	git__free(benchmark);
}

static int clone_configure(
	gitbench_benchmark_clone *benchmark,
	int argc,
	const char **argv)
{
	gitbench_opt_parser parser;
	gitbench_opt opt;

	/* the 3 local-related args should be treated as
	 * a radio group, but we just take the last value
	 * we see.
	 */
	benchmark->local = GIT_CLONE_LOCAL_AUTO;

	/* TODO decide if we want a "--bare" command line arg. */
	benchmark->bare = true;

	gitbench_opt_parser_init(&parser, clone_cmdline_opts, argv + 1, argc - 1);

	while (gitbench_opt_parser_next(&opt, &parser)) {
		if (!opt.spec) {
			fprintf(stderr, "%s: unknown argument: '%s'\n", progname, argv[parser.idx]);
			gitbench_opt_usage_fprint(stderr, progname, clone_cmdline_opts);
			return GITBENCH_EARGUMENTS;
		}

		if (strcmp(opt.spec->name, "help") == 0) {
			gitbench_opt_usage_fprint(stderr, progname, clone_cmdline_opts);
			return GITBENCH_EARGUMENTS;
		} else if (strcmp(opt.spec->name, "repository") == 0) {
			benchmark->repo_path = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->repo_path);
		} else if (strcmp(opt.spec->name, "username") == 0) {
			benchmark->username = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->username);
		} else if (strcmp(opt.spec->name, "password") == 0) {
			benchmark->password = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->password);

		} else if (strcmp(opt.spec->name, "local") == 0) {
			benchmark->local = GIT_CLONE_LOCAL;
		} else if (strcmp(opt.spec->name, "no-local") == 0) {
			benchmark->local = GIT_CLONE_NO_LOCAL;
		} else if (strcmp(opt.spec->name, "no-hardlinks") == 0) {
			benchmark->local = GIT_CLONE_LOCAL_NO_LINKS;
		}
	}

	if (!benchmark->repo_path) {
		gitbench_opt_usage_fprint(stderr, progname, clone_cmdline_opts);
		return GITBENCH_EARGUMENTS;
	}

	return 0;
}

int gitbench_benchmark_clone_init(
	gitbench_benchmark **out,
	int argc,
	const char **argv)
{
	gitbench_benchmark_clone *benchmark;
	int error;

	if ((benchmark = git__calloc(1, sizeof(gitbench_benchmark_clone))) == NULL)
		return -1;

	benchmark->base.operation_cnt = CLONE_OPERATIONS_COUNT;
	benchmark->base.operations = clone_operations;
	benchmark->base.run_fn = clone_run;
	benchmark->base.free_fn = clone_free;

	if ((error = clone_configure(benchmark, argc, argv)) < 0) {
		clone_free((gitbench_benchmark *)benchmark);
		return error;
	}

	*out = (gitbench_benchmark *)benchmark;
	return 0;
}
