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
#include "alloc.h"

const char *progname;
int verbosity = 0;

static bool profile_alloc = false;

static int gitbench_init(void);
static void gitbench_shutdown(void);
static void print_usage(FILE *out);
static int help_adapter(gitbench_benchmark **out, int argc, const char **argv);
static const gitbench_benchmark_spec *benchmark_spec_lookup(const char *name);
static int benchmark_init(gitbench_benchmark **out, const char *name, int argc, const char **argv);
static int benchmark_run(git_vector *runs, gitbench_benchmark *benchmark, size_t count);
static void report_benchmark(git_vector *runs, gitbench_benchmark *benchmark);
static void benchmark_free(git_vector *runs, gitbench_benchmark *benchmark);
static void report_alloc(void);

#ifdef GIT_WIN32
static const char *basename(const char *in);
#endif

const gitbench_benchmark_spec gitbench_benchmarks[] = {
	{ "checkout", gitbench_benchmark_checkout_init, "time the checkout of a repository" },
	{ "help", help_adapter, NULL },
	{ NULL }
};

static const gitbench_opt_spec gitbench_opts[] = {
	{ GITBENCH_OPT_SWITCH, "help",        0,   NULL,  "display help", GITBENCH_OPT_USAGE_HIDDEN },
	{ GITBENCH_OPT_VALUE,  "count",       'c', "num", "number of runs", GITBENCH_OPT_USAGE_VALUE_REQUIRED },
	{ GITBENCH_OPT_SWITCH, "verbose",     'v', NULL,  "increase the verbosity" },
	{ GITBENCH_OPT_SWITCH, "allocations", 0,   NULL,  "profile allocations" },
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
	git_vector runs = GIT_VECTOR_INIT;
	int error = 0;

	progname = basename(argv[0]);

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
		} else if (strcmp(opt.spec->name, "allocations") == 0) {
			profile_alloc = true;
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

	gitbench_try(benchmark_run(&runs, benchmark, count));

	report_benchmark(&runs, benchmark);

	if (profile_alloc)
		report_alloc();

done:
	if (error < 0) {
		const git_error *err = giterr_last();
		fprintf(stderr, "%s: %s\n", progname, err ? err->message : "unknown error");
	}

	benchmark_free(&runs, benchmark);

	gitbench_shutdown();
	return error ? 1 : 0;
}

int gitbench_init(void)
{
	gitbench_alloc_init();
	return git_libgit2_init();
}

void gitbench_shutdown(void)
{
	git_libgit2_shutdown();
	gitbench_alloc_shutdown();
}

#ifdef GIT_WIN32
const char *basename(const char *in)
{
	const char *c, *last = in;
	for (c = in; *c; c++)
		if ((*c == '/' || *c == '\\') && *(c+1)) last = c+1;
	return last;
}
#endif

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
	size_t count)
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
		run->profile_alloc = profile_alloc;

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

static void report_run(const char *desc, size_t num,
	double execute, double setup, double cleanup, double total)
{
	if (desc)
		printf("%s", desc);
	else
		printf("%" PRIuZ, num);

	printf("\t%-#f", execute);

	if (1)
		printf("\t%-#f\t%-#f\t%-#f", setup, cleanup, total);

	printf("\n");
}

void report_benchmark(git_vector *runs, gitbench_benchmark *benchmark)
{
	gitbench_run *run;
	double total_setup = 0, total_execute = 0, total_cleanup = 0, total = 0,
		average_setup, average_execute, average_cleanup, average;
	size_t i, j;

	printf("Run\tTime\tSetup\tCleanup\tTotal\n");

	git_vector_foreach(runs, i, run) {
		double run_setup = 0, run_execute = 0, run_cleanup = 0,
			run_total = run->end - run->start;

		for (j = 0; j < benchmark->operation_cnt; j++) {
			double operation_time = run->operation_data.ptr[j].end -
				run->operation_data.ptr[j].start;

			if (benchmark->operations[j].type == GITBENCH_OPERATION_SETUP)
				run_setup += operation_time;
			else if (benchmark->operations[j].type == GITBENCH_OPERATION_CLEANUP)
				run_cleanup += operation_time;
			else
				run_execute += operation_time;
		}

		report_run(NULL, i + 1,
			run_execute, run_setup, run_cleanup, run_total);

		total_setup += run_setup;
		total_execute += run_execute;
		total_cleanup += run_cleanup;

		total += (run->end - run->start);
	}

	report_run("Total", 0, total_execute, total_setup, total_cleanup, total);

	average_setup = total_setup / runs->length;
	average_execute = total_execute / runs->length;
	average_cleanup = total_cleanup / runs->length;
	average = total / runs->length;

	report_run("Average", 0, average_execute, average_setup, average_cleanup, average);
}

void benchmark_free(git_vector *runs, gitbench_benchmark *benchmark)
{
	gitbench_run *run;
	size_t i;

	if (runs) {
		git_vector_foreach(runs, i, run)
			gitbench_run_free(run);

		git_vector_free(runs);
	}

	if (benchmark)
		benchmark->free_fn(benchmark);
}

void report_alloc(void)
{
	gitbench_alloc_stat_t *stats = gitbench_alloc_stats();

	printf("Alloc count: %" PRIuZ " / Dealloc count: %" PRIuZ " / Max size: %" PRIuZ "\n",
		stats->total_alloc_count, stats->total_dealloc_count, stats->total_alloc_max);
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

	const gitbench_opt_spec gitbench_opts[] = {
		{ GITBENCH_OPT_ARG,    "benchmark", 0,   NULL,  "the benchmark to run", GITBENCH_OPT_USAGE_REQUIRED },
		{ 0 }
	};

	gitbench_opt_parser_init(&parser, gitbench_opts, argv + 1, argc - 1);

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
