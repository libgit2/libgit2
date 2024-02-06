/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "git2.h"
#include "futils.h"

#include "fuzzer_utils.h"

void fuzzer_git_abort(const char *op)
{
	const git_error *err = git_error_last();
	fprintf(stderr, "unexpected libgit error: %s: %s\n",
		op, err ? err->message : "<none>");
	abort();
}

git_repository *fuzzer_repo_init(void)
{
	git_repository *repo;

#if defined(_WIN32)
	char tmpdir[MAX_PATH], path[MAX_PATH];

	if (GetTempPath((DWORD)sizeof(tmpdir), tmpdir) == 0)
		abort();

	if (GetTempFileName(tmpdir, "lg2", 1, path) == 0)
		abort();

	if (git_futils_mkdir(path, 0700, 0) < 0)
		abort();
#else
	char path[] = "/tmp/git2.XXXXXX";

	if (mkdtemp(path) != path)
		abort();
#endif

	if (git_repository_init(&repo, path, 1) < 0)
		fuzzer_git_abort("git_repository_init");

	return repo;
}
