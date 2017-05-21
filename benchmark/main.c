/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2.h"
#include "common.h"
#include "fileops.h"
#include "vector.h"
#include "path.h"

#include "bench_util.h"
#include "opt.h"
#include "benchmark.h"
#include "run.h"

FILE *logfile = NULL;
const char *progname;
int verbosity = 0;

static bool compare_with_git_exe = false;
static int gitbench_init(void);
static void gitbench_shutdown(void);
static const char *gitbench_basename(const char *in);
static void print_usage(FILE *out);
static int help_adapter(gitbench_benchmark **out, int argc, const char **argv);
static const gitbench_benchmark_spec *benchmark_spec_lookup(const char *name);
static int benchmark_init(gitbench_benchmark **out, const char *name, int argc, const char **argv);
static int benchmark_run(git_vector *runs, gitbench_benchmark *benchmark, unsigned int count, bool use_git_exe);
static void report_logfile_header(int argc, const char **argv);
static void report_benchmark(
	const char *label,
	git_vector *runs,
	gitbench_benchmark *benchmark);

static void runs_free(git_vector *runs);
static void benchmark_free(gitbench_benchmark *benchmark);

const gitbench_benchmark_spec gitbench_benchmarks[] = {
	{ "checkout",  gitbench_benchmark_checkout_init,  "time the checkout of a repository" },
	{ "checkoutn", gitbench_benchmark_checkoutn_init, "time the checkout of a repository" }, /* new version */
	{ "clone",    gitbench_benchmark_clone_init,    "time a clone" },
	{ "merge",    gitbench_benchmark_merge_init,    "time a merge" },

	{ "help",     help_adapter, NULL },
	{ NULL }
};

static const gitbench_opt_spec gitbench_opts[] = {
	{ GITBENCH_OPT_SWITCH, "help",        0,   NULL,  "display help", GITBENCH_OPT_USAGE_HIDDEN },
	{ GITBENCH_OPT_VALUE,  "count",       'c', "num", "number of runs", GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_VALUE,  "logfile",     'l', "logfile", "write to file rather than stdout", GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_SWITCH, "verbose",     'v', NULL,  "increase the verbosity" },
	{ GITBENCH_OPT_SWITCH, "git",         'g', "git", "compare performance with git.exe" },
	{ GITBENCH_OPT_ARG,    "benchmark",   0,   NULL,  "the benchmark to run", GITBENCH_OPT_USAGE_REQUIRED },
	{ GITBENCH_OPT_ARGS,   "args",        0,   NULL,  "arguments for the benchmark" },
	{ 0 }
};

#define gitbench_try(x) if ((error = (x)) < 0) goto done;

int main(int argc, const char **argv)
{
	gitbench_opt_parser parser;
	gitbench_opt opt;
	const char *benchmark_name = NULL;
	git_vector cmd_args = GIT_VECTOR_INIT;
	gitbench_benchmark *benchmark = NULL;
	unsigned int count = 1;
	git_vector runs_lg2 = GIT_VECTOR_INIT;
	git_vector runs_git = GIT_VECTOR_INIT;
	int error = 0;

	logfile = stdout;

	progname = gitbench_basename(argv[0]);

	gitbench_try(gitbench_init());

	gitbench_try(git_vector_insert(&cmd_args, (char *)progname));

	gitbench_opt_parser_init(&parser, gitbench_opts, argv + 1, argc - 1);

	while (gitbench_opt_parser_next(&opt, &parser)) {
		if (!opt.spec) {
			gitbench_try(git_vector_insert(&cmd_args, (char *)argv[parser.idx]));
			continue;
		}

		if (strcmp(opt.spec->name, "help") == 0) {
			gitbench_try(git_vector_insert(&cmd_args, (char *)argv[parser.idx]));
		} else if (strcmp(opt.spec->name, "verbose") == 0) {
			verbosity++;
		} else if (strcmp(opt.spec->name, "git") == 0) {
			compare_with_git_exe = true;
		} else if (strcmp(opt.spec->name, "logfile") == 0) {
			logfile = fopen(opt.value, "a");
			if (logfile == NULL) {
				fprintf(stderr, "%s: cannot open logfile '%s'\n", progname, opt.value);
				print_usage(stderr);
				error = 1;
				goto done;
			}
			report_logfile_header(argc, argv);
		} else if (strcmp(opt.spec->name, "count") == 0) {
			char *end;
			long c = strtol(opt.value, &end, 10);

			if (c <= 0 || *end) {
				fprintf(stderr, "%s: invalid count '%s'\n'", progname, opt.value);
				print_usage(stderr);
				error = 1;
				goto done;
			}

			count = (unsigned int)c;
		} else if (strcmp(opt.spec->name, "benchmark") == 0) {
			benchmark_name = argv[parser.idx];
		}
	}

	if ((error = benchmark_init(&benchmark, benchmark_name, cmd_args.length, (const char **)cmd_args.contents)) < 0) {
		if (error == GITBENCH_EARGUMENTS)
			error = 1;
		goto done;
	}

	gitbench_try(benchmark_run(&runs_lg2, benchmark, count, false));
	report_benchmark("LibGit2", &runs_lg2, benchmark);

	if (compare_with_git_exe) {
		gitbench_try(benchmark_run(&runs_git, benchmark, count, true));
		report_benchmark("GitExe", &runs_git, benchmark);
	}

done:
	if (error < 0) {
		const git_error *err = giterr_last();
		fprintf(stderr, "%s: %s\n", progname, err ? err->message : "unknown error");

		if (logfile != stdout) {
			fprintf(logfile, "\n\n________________________________________________________________\n");
			fprintf(logfile, "%s: %s\n", progname, err ? err->message : "unknown error");
		}
	}

	runs_free(&runs_lg2);
	runs_free(&runs_git);
	benchmark_free(benchmark);

	gitbench_shutdown();

	if (logfile)
		fclose(logfile);
	return error ? 1 : 0;
}

int gitbench_init(void)
{
	return git_libgit2_init();
}

void gitbench_shutdown(void)
{
	git_libgit2_shutdown();
}

const char *gitbench_basename(const char *in)
{
	const char *c, *last = in;
	for (c = in; *c; c++)
		if ((*c == '/' || *c == '\\') && *(c+1)) last = c+1;
	return last;
}

int benchmark_init(gitbench_benchmark **out, const char *name, int argc, const char **argv)
{
	const gitbench_benchmark_spec *benchmark_spec = NULL;

	if (!name) {
		print_usage(stdout);
		return GITBENCH_EARGUMENTS;
	}

	if ((benchmark_spec = benchmark_spec_lookup(name)) == NULL) {
		fprintf(stderr, "%s: unknown benchmark '%s'\n", progname, name);
		print_usage(stderr);
		return GITBENCH_EARGUMENTS;
	}

	return benchmark_spec->init(out, argc, argv);
}

const gitbench_benchmark_spec *benchmark_spec_lookup(const char *name)
{
	const gitbench_benchmark_spec *spec;

	for (spec = gitbench_benchmarks; spec->name; spec++) {
		if (strcmp(spec->name, name) == 0)
			return spec;
	}

	return NULL;
}

int benchmark_run(
	git_vector *runs,
	gitbench_benchmark *benchmark,
	unsigned int count,
	bool use_git_exe)
{
	size_t i;
	int error = 0;

	assert(runs && benchmark);

	if (git_vector_init(runs, count, NULL) < 0)
		return -1;

	for (i = 0; i < count; i++) {
		gitbench_run *run;

		if (gitbench_run_init(
			&run, i+1,
			benchmark->operation_cnt,
			benchmark->operations) < 0)
			return -1;

		run->verbosity = verbosity;
		run->use_git_exe = use_git_exe;

		if (git_vector_insert(runs, run) < 0)
			return -1;
	}

	for (i = 0; i < count; i++) {
		gitbench_run *run = git_vector_get(runs, i);

		if ((error = gitbench_run_start(run)) < 0 ||
			(error = benchmark->run_fn(benchmark, run)) < 0 ||
			(error = gitbench_run_finish(run)) < 0)
			break;
	}

	return error;
}

static void report_logfile_header(int argc, const char **argv)
{
	int k;

	fprintf(logfile, "\n");
	fprintf(logfile, "################################################################\n");
	fprintf(logfile, "%s", progname);
	for (k=1; k<argc; k++)
		fprintf(logfile, " %s", argv[k]);
	fprintf(logfile, "\n");
	fprintf(logfile, "\n");
}

void report_benchmark(
	const char *label,
	git_vector *runs,
	gitbench_benchmark *benchmark)
{
	double *tally = NULL;
	int *ran_op = NULL;
	gitbench_run *run;
	int count_runs = (int)git_vector_length(runs);
	size_t i, j;

	tally = (double *)git__calloc(benchmark->operation_cnt + 1, sizeof(double));
	ran_op = (int *)git__calloc(benchmark->operation_cnt, sizeof(int));

	/* Column headers */

	fprintf(logfile, "\n");
	fprintf(logfile, "%-15s", label);
	for (j = 0; j < benchmark->operation_cnt; j++)
		fprintf(logfile, " %13s", benchmark->operations[j].description);
	fprintf(logfile, " : %10s\n", "TOTAL");

	/* Each run (--count x) */

	git_vector_foreach(runs, i, run) {
		bool multiple = false;
		double run_total = run->overall_end - run->overall_start;

		/* Data for each run */

		fprintf(logfile, "%-15d", (int)(i+1));
		for (j = 0; j < benchmark->operation_cnt; j++) {
			if (run->operation_data.ptr[j].ran_count) {
				/* We have data for this "column". */

				double time_j = run->operation_data.ptr[j].op_sum;
				fprintf(logfile, " %10.3f/", time_j);
				if (run->operation_data.ptr[j].ran_count > 1)
					fprintf(logfile, "%02d", run->operation_data.ptr[j].ran_count);
				else
					fprintf(logfile, "__");

				tally[j] += time_j;
				ran_op[j] += run->operation_data.ptr[j].ran_count;

				if (run->operation_data.ptr[j].ran_count > 1)
					multiple = true;

			} else {
				fprintf(logfile, " %13s", " ");
			}
		}
		fprintf(logfile, " : %10.3f\n", run_total);

		tally[benchmark->operation_cnt] += run_total;

		/* If any column in this row had a repeat count, report sub line with average. */

		if (multiple) {
			fprintf(logfile, "%15s", "(sub-avg)");
			for (j = 0; j < benchmark->operation_cnt; j++) {
				if (run->operation_data.ptr[j].ran_count > 1) {

					double time_j = run->operation_data.ptr[j].op_sum;
					double avg_j = (time_j / run->operation_data.ptr[j].ran_count);
					fprintf(logfile, " %10.3f   ", avg_j);
				} else {
					fprintf(logfile, " %13s", " ");
				}
			}
			fprintf(logfile, "\n");
		}

		fprintf(logfile, "\n");
	}

	/* Total of all runs */

	fprintf(logfile, "%-15s", "Total");
	for (j = 0; j < benchmark->operation_cnt; j++) {
		if (ran_op[j])
			fprintf(logfile, " %10.3f/%02d", tally[j], ran_op[j]);
		else
			fprintf(logfile, " %13s", " ");
	}
	fprintf(logfile, " : %10.3f\n", tally[benchmark->operation_cnt]);
	fprintf(logfile, "\n");

	/* Average of the runs */

	fprintf(logfile, "%-15s", "Average");
	for (j = 0; j < benchmark->operation_cnt; j++) {
		if (ran_op[j])
			fprintf(logfile, " %10.3f   ", (tally[j] / ran_op[j]));
		else
			fprintf(logfile, " %13s", " ");
	}
	fprintf(logfile, " : %10.3f\n", (tally[benchmark->operation_cnt] / count_runs));
	fprintf(logfile, "\n");

	git__free(tally);
}

void runs_free(git_vector *runs)
{
	gitbench_run *run;
	size_t i;

	if (runs) {
		git_vector_foreach(runs, i, run)
			gitbench_run_free(run);

		git_vector_free(runs);
	}
}

void benchmark_free(gitbench_benchmark *benchmark)
{
	if (benchmark)
		benchmark->free_fn(benchmark);
}

void print_usage(FILE *out)
{
	const gitbench_benchmark_spec *b;

	gitbench_opt_usage_fprint(out, progname, gitbench_opts);
	fprintf(out, "\n");

	fprintf(out, "Available benchmarks are:\n");

	for (b = gitbench_benchmarks; b->name; b++) {
		if (b->description)
			fprintf(out, "    %-10s %s\n", b->name, b->description);
	}
}

int help_adapter(gitbench_benchmark **out, int argc, const char **argv)
{
	gitbench_opt_parser parser;
	gitbench_opt opt;
	const char *benchmark_name = NULL;
	gitbench_benchmark *benchmark = NULL;
	const char *help_args[] = { NULL, "--help" };

	const gitbench_opt_spec gitbench_opts__help_adapter[] = {
		{ GITBENCH_OPT_ARG,    "benchmark", 0,   NULL,  "the benchmark to run", GITBENCH_OPT_USAGE_REQUIRED },
		{ 0 }
	};

	gitbench_opt_parser_init(&parser, gitbench_opts__help_adapter, argv + 1, argc - 1);

	while (gitbench_opt_parser_next(&opt, &parser)) {
		if (opt.spec && strcmp(opt.spec->name, "benchmark") == 0) {
			benchmark_name = opt.value;
			break;
		}
	}

	if (!benchmark_name) {
		print_usage(stdout);
		return GITBENCH_EARGUMENTS;
	}

	help_args[0] = benchmark_name;

	if (benchmark_init(&benchmark, benchmark_name, 2, help_args) == 0) {
		fprintf(stderr, "%s: no help available for '%s'\n",
				progname, benchmark_name);
		benchmark->free_fn(benchmark);
	}

	*out = NULL;
	return GITBENCH_EARGUMENTS;
}
