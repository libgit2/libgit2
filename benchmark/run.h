/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef RUN_H
#define RUN_H

#include "common.h"
#include "vector.h"
#include "array.h"
#include "operation.h"

typedef struct {
	gitbench_operation_spec *spec;
	int ran_count;
	double op_start;
	double op_end;
	double op_sum;
} gitbench_run_operationdata;

/** Specification for a benchmark command. */
typedef struct {
	size_t id;
	int verbosity;

	/* use git.exe for this run */
	unsigned int use_git_exe:1;

	const char *tempdir;

	/* start and end of the overall run */
	double overall_start;
	double overall_end;
	git_array_t(gitbench_run_operationdata) operation_data;
	unsigned int current_operation;
} gitbench_run;

/** Allocate a run. */
extern int gitbench_run_init(
	gitbench_run **run,
	size_t id,
	size_t operation_cnt,
	gitbench_operation_spec operations[]);

/** Start an overall run. */
extern int gitbench_run_start(gitbench_run *run);

/** Finish an overall run. */
extern int gitbench_run_finish(gitbench_run *run);

/** Start a single operation within a run. */
extern int gitbench_run_start_operation(gitbench_run *run, unsigned int operation);

/** Finish a single operation within a run. */
extern int gitbench_run_finish_operation(gitbench_run *run);

/** Free a run. */
extern void gitbench_run_free(gitbench_run *run);

#endif
