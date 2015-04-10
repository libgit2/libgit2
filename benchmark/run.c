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
#include "alloc.h"

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

	memset(run->operation_data.ptr, 0, sizeof(run->operation_data.ptr));
	run->operation_data.size = operation_cnt;

	for (i = 0; i < operation_cnt; i++)
		run->operation_data.ptr[i].spec = &operations[i];

	*out = run;
	return 0;
}

int gitbench_run_start(gitbench_run *run)
{
	assert(run);
	assert(run->start == 0);

	if (gitbench__create_tempdir((char **)&run->tempdir) < 0)
		return -1;

	if (run->verbosity)
		printf(": Starting run %" PRIuZ "\n", run->id);

	run->start = git__timer();
	return 0;
}

int gitbench_run_finish(gitbench_run *run)
{
	assert(run);
	assert(run->end == 0);

	run->end = git__timer();

	if (run->verbosity)
		printf(": Finished run %" PRIuZ "\n", run->id);

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

	assert(!opdata->ran);

	if (run->profile_alloc) {
		gitbench_alloc_start();

		if (run->verbosity)
			printf("::: Replacing allocation functions...\n");
	}

	if (run->verbosity) {
		const char *type = "operation";

		if (opdata->spec->type == GITBENCH_OPERATION_SETUP)
			type = "setup";
		else if (opdata->spec->type == GITBENCH_OPERATION_CLEANUP)
			type = "cleanup";

		printf("::: Starting %s: %s\n", type, opdata->spec->description);
	}

	run->current_operation = opcode;

	opdata->ran = 1;
	opdata->start = git__timer();

	return 0;
}

int gitbench_run_finish_operation(gitbench_run *run)
{
	gitbench_run_operationdata *opdata;

	assert(run);
	assert(run->current_operation != RUN_OPERATION_NONE);

	opdata = git_array_get(run->operation_data, run->current_operation);
	opdata->end = git__timer();

	run->current_operation = RUN_OPERATION_NONE;

	if (run->verbosity) {
		const char *type = "operation";

		if (opdata->spec->type == GITBENCH_OPERATION_SETUP)
			type = "setup";
		else if (opdata->spec->type == GITBENCH_OPERATION_CLEANUP)
			type = "cleanup";

		printf("::: Finished %s: %s (total=%f seconds)\n",
			type, opdata->spec->description, (opdata->end - opdata->start));
	}

	if (run->profile_alloc) {
		gitbench_alloc_stop();
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
