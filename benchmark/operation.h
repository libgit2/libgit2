/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef OPERATION_H
#define OPERATION_H

enum gitbench_operation_t {
	GITBENCH_OPERATION_SETUP = 1,
	GITBENCH_OPERATION_EXECUTE = 2,
	GITBENCH_OPERATION_CLEANUP = 3
};

typedef struct gitbench_operation_spec {
	unsigned int code;
	unsigned int type;
	const char *description;
} gitbench_operation_spec;

#endif
