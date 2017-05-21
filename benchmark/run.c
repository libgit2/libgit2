/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "fileops.h"
#include "run.h"
#include "bench_util.h"

#define RUN_OPERATION_NONE UINT_MAX

int gitbench_run_init(
	gitbench_run **out,
	size_t id,
	size_t operation_cnt,
	gitbench_operation_spec operations[])
{
	gitbench_run *run;
	size_t i;

	assert(out);
	assert(operation_cnt != RUN_OPERATION_NONE);

	*out = NULL;

	if ((run = git__calloc(1, sizeof(gitbench_run))) == NULL)
		return -1;

	git_array_init_to_size(run->operation_data, operation_cnt);
	GITERR_CHECK_ALLOC(run->operation_data.ptr);

	run->id = id;
	run->current_operation = RUN_OPERATION_NONE;

	memset(run->operation_data.ptr, 0, sizeof(*run->operation_data.ptr));
	run->operation_data.size = operation_cnt;

	for (i = 0; i < operation_cnt; i++)
		run->operation_data.ptr[i].spec = &operations[i];

	*out = run;
	return 0;
}

int gitbench_run_start(gitbench_run *run)
{
	assert(run);
	assert(run->overall_start == 0);

	if (gitbench__create_tempdir((char **)&run->tempdir) < 0)
		return -1;

	if (run->verbosity)
		fprintf(logfile, ": Starting run %" PRIuZ "\n", run->id);

	run->overall_start = git__timer();
	return 0;
}

int gitbench_run_finish(gitbench_run *run)
{
	assert(run);
	assert(run->overall_end == 0);

	run->overall_end = git__timer();

	if (run->verbosity)
		fprintf(logfile, ": Finished run %" PRIuZ "\n", run->id);

	git_futils_rmdir_r(run->tempdir, NULL, GIT_RMDIR_REMOVE_FILES);

	return 0;
}

int gitbench_run_start_operation(gitbench_run *run, unsigned int opcode)
{
	gitbench_run_operationdata *opdata;

	assert(run);
	assert(opcode < run->operation_data.size);
	assert(run->current_operation == RUN_OPERATION_NONE);

	opdata = git_array_get(run->operation_data, opcode);

	if (run->verbosity) {
		const char *type = "operation";
		fprintf(logfile, "::: Starting %s: %s\n", type, opdata->spec->description);
	}

	run->current_operation = opcode;

	opdata->ran_count++;
	opdata->op_start = git__timer();

	return 0;
}

int gitbench_run_finish_operation(gitbench_run *run)
{
	gitbench_run_operationdata *opdata;

	assert(run);

	/* Allow multiple finishes for error handling. */
	if (run->current_operation == RUN_OPERATION_NONE)
		return 0;

	opdata = git_array_get(run->operation_data, run->current_operation);
	opdata->op_end = git__timer();
	opdata->op_sum += (opdata->op_end - opdata->op_start);

	run->current_operation = RUN_OPERATION_NONE;

	if (run->verbosity) {
		const char *type = "operation";
		fprintf(logfile, "::: Finished %s: %s (total=%f seconds)\n",
			type, opdata->spec->description, (opdata->op_end - opdata->op_start));
	}

	return 0;
}

void gitbench_run_free(gitbench_run *run)
{
	if (!run)
		return;

	git__free((char *)run->tempdir);
	git__free(run);
}
