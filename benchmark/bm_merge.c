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
#include "shell.h"
#include "operation.h"
#include "benchmark.h"

typedef struct gitbench_benchmark_merge {
	gitbench_benchmark base;

	char *repo_url;
	char *ref_name_checkout;
	char *ref_name_merge;

	unsigned int autocrlf:1;

	int status_count;

} gitbench_benchmark_merge;

enum merge_operation_t {
	MERGE_OPERATION_EXE_CLONE = 0,
	MERGE_OPERATION_EXE_CHECKOUT,
	MERGE_OPERATION_EXE_MERGE,
	MERGE_OPERATION_LG2_MERGE,
	MERGE_OPERATION_EXE_STATUS,
	MERGE_OPERATION_LG2_STATUS
};

/* descriptions must be 10 chars or less for column header alignment. */
static gitbench_operation_spec merge_operations[] = {
	{ MERGE_OPERATION_EXE_CLONE,    "ExeClone" },
	{ MERGE_OPERATION_EXE_CHECKOUT, "ExeCO" },
	{ MERGE_OPERATION_EXE_MERGE,    "ExeMerge" },
	{ MERGE_OPERATION_LG2_MERGE,    "Lg2Merge" },
	{ MERGE_OPERATION_EXE_STATUS,   "ExeStatus" },
	{ MERGE_OPERATION_LG2_STATUS,   "Lg2Status" },
};
#define MERGE_OPERATIONS_COUNT (sizeof(merge_operations) / sizeof(merge_operations[0]))

static const gitbench_opt_spec merge_cmdline_opts[] = {
	{ GITBENCH_OPT_SWITCH, "help",           0, NULL,      "display help",   GITBENCH_OPT_USAGE_HIDDEN },
	{ GITBENCH_OPT_SWITCH, "autocrlf",       0, NULL,      "turn on core.autocrlf=true" },
	{ GITBENCH_OPT_VALUE,  "ref_checkout", 'r', "refname", "the reference to checkout", GITBENCH_OPT_USAGE_REQUIRED | GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_VALUE,  "ref_merge",    'm', "refname", "the reference to merge in", GITBENCH_OPT_USAGE_REQUIRED | GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_VALUE,  "status",       's', "count",   "times to run status aftwards", GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_ARG,    "repository",     0, NULL,      "the repository",            GITBENCH_OPT_USAGE_REQUIRED },
	{ 0 }
};


/**
 * Clone the requested repo to TMP.
 * DO NOT let clone checkout the default HEAD.
 * Fix merge.renameLimit.
 */
static int _init_exe_clone(
	gitbench_benchmark_merge *benchmark,
	gitbench_run *run,
	const char *wd)
{
	const char * argv[10] = {0};
	int k;
	int error;

	gitbench_run_start_operation(run, MERGE_OPERATION_EXE_CLONE);

	k = 0;
	argv[k++] = BM_GIT_EXE;
	argv[k++] = "clone";
	argv[k++] = "--quiet";
	argv[k++] = "--no-checkout";
	argv[k++] = "--local";
	argv[k++] = benchmark->repo_url;
	argv[k++] = wd;
	argv[k++] = 0;

	if ((error = gitbench_shell(argv, NULL, NULL)) < 0)
		goto done;

	k = 0;
	argv[k++] = BM_GIT_EXE;
	argv[k++] = "config";
	argv[k++] = "core.autocrlf";
	argv[k++] = ((benchmark->autocrlf) ? "true" : "false");
	argv[k++] = 0;

	if ((error = gitbench_shell(argv, wd, NULL)) < 0)
		return error;

	k = 0;
	argv[k++] = BM_GIT_EXE;
	argv[k++] = "config";
	argv[k++] = "merge.renameLimit";
	argv[k++] = "999999";
	argv[k++] = 0;

	if ((error = gitbench_shell(argv, wd, NULL)) < 0)
		goto done;

done:
	gitbench_run_finish_operation(run);
	return error;
}

/**
 * Checkout the requested commit in detached head state.
 */
static int _init_exe_checkout(
	gitbench_benchmark_merge *benchmark,
	gitbench_run *run,
	const char *wd)
{
	const char * argv[10] = {0};
	int k;
	int error;

	gitbench_run_start_operation(run, MERGE_OPERATION_EXE_CHECKOUT);

	k = 0;
	argv[k++] = BM_GIT_EXE;
	argv[k++] = "checkout";
	argv[k++] = "--quiet";
	argv[k++] = "-B";
	argv[k++] = "bm";
	argv[k++] = benchmark->ref_name_checkout;
	argv[k++] = 0;

	if ((error = gitbench_shell(argv, wd, NULL)) < 0)
		goto done;

done:
	gitbench_run_finish_operation(run);
	return error;
}

static int _do_core_setup(
	git_buf *wd_path,
	gitbench_benchmark_merge *benchmark,
	gitbench_run *run)
{
	int error;

	if ((error = git_buf_joinpath(wd_path, run->tempdir, "wd")) < 0)
		return error;
	if ((error = git_futils_mkdir(wd_path->ptr, NULL, 0700, GIT_MKDIR_VERIFY_DIR)) < 0)
		return error;

	return error;
}

static int _do_lg2_merge(
	gitbench_benchmark_merge *benchmark,
	gitbench_run *run,
	const char *wd)
{
	git_repository *repo = NULL;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
	git_annotated_commit *ac[1] = { NULL };
	git_object *obj = NULL;
	int error;

	gitbench_run_start_operation(run, MERGE_OPERATION_LG2_MERGE);

	if ((error = git_repository_open(&repo, wd)) < 0)
		goto done;
	if ((error = git_revparse_single(&obj, repo, benchmark->ref_name_merge)) < 0)
		goto done;
	if ((error = git_annotated_commit_lookup(&ac[0], repo, git_object_id(obj))) < 0)
		goto done;

	error = git_merge(repo, (const git_annotated_commit **)ac, 1,
					  &merge_opts, &checkout_opts);

done:
	gitbench_run_finish_operation(run);
	git_annotated_commit_free(ac[0]);
	git_object_free(obj);
	git_repository_free(repo);
	return error;
}

static int _do_exe_merge(
	gitbench_benchmark_merge *benchmark,
	gitbench_run *run,
	const char *wd)
{
	const char * argv[10] = {0};
	int exit_status;
	int result;
	int k = 0;

	argv[k++] = BM_GIT_EXE;
	argv[k++] = "merge";
	argv[k++] = "--no-commit";
	argv[k++] = "--quiet";
	argv[k++] = benchmark->ref_name_merge;
	argv[k++] = 0;

	gitbench_run_start_operation(run, MERGE_OPERATION_EXE_MERGE);
	result = gitbench_shell(argv, wd, &exit_status);
	gitbench_run_finish_operation(run);

	/* "git merge" exits with 1 when there are merge conflicts
	 * OR when the target commit cannot be found.  (We get 128
	 * or 129 for usage errors.)
	 *
	 * If we get a 1, assume a conflict.  This implies that
	 * merge finished and we can continue with the timing.
	 * So we ignore the sanitized result and key off the
	 * actual exit status instead.
	 */
	if (exit_status == 1)
		fprintf(logfile, "::::: git-merge.exe exited with 1; assuming conflicts\n");
	if ((exit_status == 0) || (exit_status == 1))
		return 0;
	return result;
}


static int _do_lg2_status(
	gitbench_benchmark_merge *benchmark,
	gitbench_run *run,
	const char *wd)
{
	git_repository *repo = NULL;
	git_status_list *status = NULL;
	git_status_options status_opts = GIT_STATUS_OPTIONS_INIT;
	int error;
	int x;

	status_opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	status_opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

	if ((error = git_repository_open(&repo, wd)) < 0)
		goto done;

	for (x = 0; x < benchmark->status_count; x++) {
		gitbench_run_start_operation(run, MERGE_OPERATION_LG2_STATUS);
		error = git_status_list_new(&status, repo, &status_opts);
		gitbench_run_finish_operation(run);
		if (error < 0)
			goto done;
	}

done:
	gitbench_run_finish_operation(run);
	git_status_list_free(status);
	git_repository_free(repo);
	return error;
}

static int _do_exe_status(
	gitbench_benchmark_merge *benchmark,
	gitbench_run *run,
	const char *wd)
{
	const char * argv[10] = {0};
	int k = 0;
	int error;
	int x;

	argv[k++] = BM_GIT_EXE;
	argv[k++] = "status";
	argv[k++] = "--porcelain";
	argv[k++] = "--branch";
	argv[k++] = 0;

	for (x = 0; x < benchmark->status_count; x++) {
		gitbench_run_start_operation(run, MERGE_OPERATION_EXE_STATUS);
		error = gitbench_shell(argv, wd, NULL);
		gitbench_run_finish_operation(run);
		if (error < 0)
			goto done;
	}

done:
	gitbench_run_finish_operation(run);
	return error;
}


static int merge_run(gitbench_benchmark *b, gitbench_run *run)
{
	gitbench_benchmark_merge *benchmark = (gitbench_benchmark_merge *)b;
	git_buf wd_path = GIT_BUF_INIT;
	int error;

	if ((error = _do_core_setup(&wd_path, benchmark, run)) < 0)
		goto done;

	// TODO Consider having lg2-based version of clone and checkout.
	if ((error = _init_exe_clone(benchmark, run, wd_path.ptr)) < 0)
		goto done;
	if ((error = _init_exe_checkout(benchmark, run, wd_path.ptr)) < 0)
		goto done;

	if (run->use_git_exe) {
		if ((error = _do_exe_merge(benchmark, run, wd_path.ptr)) < 0)
			goto done;
	} else {
		if ((error = _do_lg2_merge(benchmark, run, wd_path.ptr)) < 0)
			goto done;
	}

	/* Always run both version of status since we can.
	 * Note that there is probably a minor penalty for
	 * being first here since that one may have to re-write
	 * the index. I'm going to average a few runs to smooth
	 * this out.
	 */
	if ((error = _do_exe_status(benchmark, run, wd_path.ptr)) < 0)
		goto done;
	if ((error = _do_lg2_status(benchmark, run, wd_path.ptr)) < 0)
		goto done;

done:
	git_buf_free(&wd_path);
	return error;
}

static void merge_free(gitbench_benchmark *b)
{
	gitbench_benchmark_merge *benchmark = (gitbench_benchmark_merge *)b;

	if (!b)
		return;

	git__free(benchmark->repo_url);
	git__free(benchmark->ref_name_checkout);
	git__free(benchmark->ref_name_merge);
	git__free(benchmark);
}

static int merge_configure(
	gitbench_benchmark_merge *benchmark,
	int argc,
	const char **argv)
{
	gitbench_opt_parser parser;
	gitbench_opt opt;

	gitbench_opt_parser_init(&parser, merge_cmdline_opts, argv + 1, argc - 1);

	while (gitbench_opt_parser_next(&opt, &parser)) {
		if (!opt.spec) {
			fprintf(stderr, "%s: unknown argument: '%s'\n", progname, argv[parser.idx]);
			gitbench_opt_usage_fprint(stderr, progname, merge_cmdline_opts);
			return GITBENCH_EARGUMENTS;
		}

		if (strcmp(opt.spec->name, "help") == 0) {
			gitbench_opt_usage_fprint(stderr, progname, merge_cmdline_opts);
			return GITBENCH_EARGUMENTS;
		} else if (strcmp(opt.spec->name, "autocrlf") == 0) {
			benchmark->autocrlf = 1;
		} else if (strcmp(opt.spec->name, "repository") == 0) {
			benchmark->repo_url = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->repo_url);
		} else if (strcmp(opt.spec->name, "ref_checkout") == 0) {
			benchmark->ref_name_checkout = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->ref_name_checkout);
		} else if (strcmp(opt.spec->name, "ref_merge") == 0) {
			benchmark->ref_name_merge = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->ref_name_merge);
		} else if (strcmp(opt.spec->name, "status") == 0) {
			char *end;
			long c = strtol(opt.value, &end, 10);
			if (c <= 0 || *end) {
				fprintf(stderr, "%s: invalid status count '%s'\n", progname, opt.value);
				gitbench_opt_usage_fprint(stderr, progname, merge_cmdline_opts);
				return GITBENCH_EARGUMENTS;
			}
			benchmark->status_count = c;
		}
	}

	if (benchmark->status_count == 0)
		benchmark->status_count = 1;

	if (!benchmark->repo_url ||
		!benchmark->ref_name_checkout ||
		!benchmark->ref_name_merge) {
		gitbench_opt_usage_fprint(stderr, progname, merge_cmdline_opts);
		return GITBENCH_EARGUMENTS;
	}

	return 0;
}

int gitbench_benchmark_merge_init(
	gitbench_benchmark **out,
	int argc,
	const char **argv)
{
	gitbench_benchmark_merge *benchmark;
	int error;

	if ((benchmark = git__calloc(1, sizeof(gitbench_benchmark_merge))) == NULL)
		return -1;

	benchmark->base.operation_cnt = MERGE_OPERATIONS_COUNT;
	benchmark->base.operations = merge_operations;
	benchmark->base.run_fn = merge_run;
	benchmark->base.free_fn = merge_free;

	if ((error = merge_configure(benchmark, argc, argv)) < 0) {
		merge_free((gitbench_benchmark *)benchmark);
		return error;
	}

	*out = (gitbench_benchmark *)benchmark;
	return 0;
}
