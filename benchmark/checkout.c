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

typedef struct gitbench_benchmark_checkout {
	gitbench_benchmark base;

	char *repo_path;
	char *ref_name;
	unsigned int autocrlf:1;
} gitbench_benchmark_checkout;

enum checkout_operation_t {
	CHECKOUT_OPERATION_SETUP = 0,
	CHECKOUT_OPERATION_SETUP_FILTERS,
	CHECKOUT_OPERATION_CHECKOUT,
	CHECKOUT_OPERATION_CLEANUP
};

static gitbench_operation_spec checkout_operations[] = {
	{ CHECKOUT_OPERATION_SETUP, GITBENCH_OPERATION_SETUP, "open repository" },
	{ CHECKOUT_OPERATION_SETUP_FILTERS, GITBENCH_OPERATION_SETUP, "configure cr/lf filters" },
	{ CHECKOUT_OPERATION_CHECKOUT, GITBENCH_OPERATION_EXECUTE, "execute checkout" },
	{ CHECKOUT_OPERATION_CLEANUP, GITBENCH_OPERATION_CLEANUP, "close repository" },
};
#define CHECKOUT_OPERATIONS_COUNT 4

static const gitbench_opt_spec checkout_cmdline_opts[] = {
	{ GITBENCH_OPT_SWITCH, "help",       0,   NULL,      NULL },
	{ GITBENCH_OPT_ARG,    "repository", 0,   NULL,      "the repository to checkout", GITBENCH_OPT_USAGE_REQUIRED },
	{ GITBENCH_OPT_VALUE,  "reference",  'r', "refname", "the reference to checkout", GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_SWITCH, "autocrlf",   0,   NULL,      "turn on core.autocrlf=true" },
	{ 0 }
};

int checkout_run(gitbench_benchmark *b, gitbench_run *run)
{
	git_repository *repo = NULL;
	git_oid id;
	git_object *obj = NULL;
	git_config *config = NULL;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	gitbench_benchmark_checkout *benchmark = (gitbench_benchmark_checkout *)b;
	git_buf wd_path = GIT_BUF_INIT, config_path = GIT_BUF_INIT;
	const char *ref_name = "HEAD";
	int error;

	/* setup repository */
	gitbench_run_start_operation(run, CHECKOUT_OPERATION_SETUP);

	if (benchmark->ref_name)
		ref_name = benchmark->ref_name;

	if ((error = git_repository_open(&repo, benchmark->repo_path)) < 0 ||
		(error = git_reference_name_to_id(&id, repo, ref_name)) < 0 ||
		(error = git_object_lookup(&obj, repo, &id, GIT_OBJ_ANY)) < 0 ||
		(error = git_buf_joinpath(&wd_path, run->tempdir, "workdir")) < 0 ||
		(error = git_futils_mkdir(wd_path.ptr, NULL, 0700, GIT_MKDIR_VERIFY_DIR)) < 0 ||
		(error = git_repository_set_workdir(repo, wd_path.ptr, 0)) < 0 ||
		(error = git_config_new(&config)) < 0) {
		gitbench_run_finish_operation(run);
		goto done;
	}

	git_repository_set_index(repo, NULL);
	git_repository_set_config(repo, config);

	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	gitbench_run_finish_operation(run);

	if (benchmark->autocrlf) {
		gitbench_run_start_operation(run, CHECKOUT_OPERATION_SETUP_FILTERS);

		if ((error = git_buf_joinpath(&config_path, run->tempdir, ".config")) == 0 &&
			(error = git_config_add_file_ondisk(config, config_path.ptr, GIT_CONFIG_LEVEL_LOCAL, 0)) == 0)
			error = git_config_set_bool(config, "core.autocrlf", true);

		gitbench_run_finish_operation(run);

		if (error < 0)
			goto done;
	}

	/* do the checkout */
	gitbench_run_start_operation(run, CHECKOUT_OPERATION_CHECKOUT);
	error = git_checkout_tree(repo, obj, &opts);
	gitbench_run_finish_operation(run);

done:
	/* cleanup */
	gitbench_run_start_operation(run, CHECKOUT_OPERATION_CLEANUP);
	git_object_free(obj);
	git_config_free(config);
	git_repository_free(repo);
	git_buf_free(&wd_path);
	git_buf_free(&config_path);
	gitbench_run_finish_operation(run);

	return error;
}

void checkout_free(gitbench_benchmark *b)
{
	gitbench_benchmark_checkout *benchmark = (gitbench_benchmark_checkout *)b;

	if (!b)
		return;

	git__free(benchmark->repo_path);
	git__free(benchmark->ref_name);
	git__free(benchmark);
}

int checkout_configure(
	gitbench_benchmark_checkout *benchmark,
	int argc,
	const char **argv)
{
	gitbench_opt_parser parser;
	gitbench_opt opt;

	gitbench_opt_parser_init(&parser, checkout_cmdline_opts, argv + 1, argc - 1);

	while (gitbench_opt_parser_next(&opt, &parser)) {
		if (!opt.spec) {
			fprintf(stderr, "%s: unknown argument: '%s'\n", progname, argv[parser.idx]);
			gitbench_opt_usage_fprint(stderr, progname, checkout_cmdline_opts);
			return GITBENCH_EARGUMENTS;
		}

		if (strcmp(opt.spec->name, "help") == 0) {
			gitbench_opt_usage_fprint(stderr, progname, checkout_cmdline_opts);
			return GITBENCH_EARGUMENTS;
		} else if (strcmp(opt.spec->name, "repository") == 0) {
			benchmark->repo_path = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->repo_path);
		} else if (strcmp(opt.spec->name, "reference") == 0) {
			benchmark->ref_name = git__strdup(opt.value);
			GITERR_CHECK_ALLOC(benchmark->ref_name);
		} else if (strcmp(opt.spec->name, "autocrlf") == 0) {
			benchmark->autocrlf = 1;
		}
	}

	if (!benchmark->repo_path) {
		gitbench_opt_usage_fprint(stderr, progname, checkout_cmdline_opts);
		return GITBENCH_EARGUMENTS;
	}

	return 0;
}

int gitbench_benchmark_checkout_init(
	gitbench_benchmark **out,
	int argc,
	const char **argv)
{
	gitbench_benchmark_checkout *benchmark;
	int error;

	if ((benchmark = git__calloc(1, sizeof(gitbench_benchmark_checkout))) == NULL)
		return -1;

	benchmark->base.operation_cnt = CHECKOUT_OPERATIONS_COUNT;
	benchmark->base.operations = checkout_operations;
	benchmark->base.run_fn = checkout_run;
	benchmark->base.free_fn = checkout_free;

	if ((error = checkout_configure(benchmark, argc, argv)) < 0) {
		checkout_free((gitbench_benchmark *)benchmark);
		return error;
	}

	*out = (gitbench_benchmark *)benchmark;
	return 0;
}
