/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "vector.h"
#include "atexit.h"

static git_vector rollbacks;

int git_atexit_global_init(void)
{
	return git_vector_init(&rollbacks, 0, NULL);
}

int git__atexit_register(git__atexit *atexit)
{
	return git_vector_insert(&rollbacks, atexit);
}

int git__atexit_unregister(git__atexit *atexit)
{
	int error;
	size_t pos;

	if ((error = git_vector_search(&pos, &rollbacks, atexit)) < 0)
		return error;

	return git_vector_remove(&rollbacks, pos);
}

int git_atexit(void)
{
	size_t i;
	int error;
	git__atexit *atexit;

	git_vector_foreach(&rollbacks, i, atexit) {
		if ((error = atexit->execute(atexit)) < 0)
			return error;
	}

	return 0;
}
