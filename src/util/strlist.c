/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>

#include "git2_util.h"
#include "vector.h"
#include "strlist.h"

int git_strlist_copy(char ***out, const char **in, size_t len)
{
	char **dup;
	size_t i;

	dup = git__calloc(len, sizeof(char *));
	GIT_ERROR_CHECK_ALLOC(dup);

	for (i = 0; i < len; i++) {
		dup[i] = git__strdup(in[i]);
		GIT_ERROR_CHECK_ALLOC(dup[i]);
	}

	*out = dup;
	return 0;
}

void git_strlist_free(char **strings, size_t len)
{
	size_t i;

	if (!strings)
		return;

	for (i = 0; i < len; i++)
		git__free(strings[i]);

	git__free(strings);
}
