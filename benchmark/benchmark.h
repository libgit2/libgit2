/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "operation.h"
#include "run.h"

typedef struct gitbench_benchmark gitbench_benchmark;

struct gitbench_benchmark {
	gitbench_operation_spec *operations;
	size_t operation_cnt;
	int (*run_fn)(gitbench_benchmark *benchmark, gitbench_run *run);
	void (*free_fn)(gitbench_benchmark *benchmark);
};


/** Specification for a benchmark command. */
typedef struct {
	/** Name of the benchmark option. */
	const char *name;

	/** The initialization function to execute. */
	int (*init)(gitbench_benchmark **out, int argc, const char **argv);

	/** The description of the benchmark. */
	const char *description;
} gitbench_benchmark_spec;

/* Benchmark initializers */
extern int gitbench_benchmark_checkout_init(
	gitbench_benchmark **out,
	int argc,
	const char **argv);

extern int gitbench_benchmark_checkoutn_init(
	gitbench_benchmark **out,
	int argc,
	const char **argv);

extern int gitbench_benchmark_clone_init(
	gitbench_benchmark **out,
	int argc,
	const char **argv);

extern int gitbench_benchmark_merge_init(
	gitbench_benchmark **out,
	int argc,
	const char **argv);

#endif
