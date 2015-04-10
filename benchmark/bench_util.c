/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <fcntl.h>
#include "common.h"
#include "fileops.h"
#include "buffer.h"

static int tempdir_isvalid(const char *path)
{
	struct stat st;

	if (p_stat(path, &st) != 0)
		return 0;

	if (!S_ISDIR(st.st_mode))
		return 0;

	return (access(path, W_OK) == 0);
}

int gitbench__create_tempdir(char **out)
{
	git_buf tempdir = GIT_BUF_INIT;
	static const char *tempname = "libgit2_bench_XXXXXX";
	char *root;

#ifdef GIT_WIN32
	char winroot[MAX_PATH];
	DWORD winroot_len;

	if ((winroot_len = GetTempPath(MAX_PATH, winroot)) == 0 ||
		winroot_len > MAX_PATH || !tempdir_isvalid(winroot)) {
		giterr_set(GITERR_OS, "could not determine temporary path");
		return -1;
	}

	root = winroot;

	git_path_mkposix(root);
#else
	if ((root = getenv("TMPDIR")) == NULL || !tempdir_isvalid(root)) {
		giterr_set(GITERR_OS, "could not determine temporary path");
		return -1;
	}
#endif

	if (git_buf_joinpath(&tempdir, root, tempname) < 0)
		return -1;

#ifdef GIT_WIN32
	if (_mktemp_s(tempdir.ptr, tempdir.size+1) != 0 ||
		p_mkdir(tempdir.ptr, 0700) != 0) {
		giterr_set(GITERR_OS, "could not determine temporary path");
		return -1;
	}
#else
	if (mkdtemp(tempdir.ptr) == NULL)
		return -1;
#endif

	*out = git_buf_detach(&tempdir);
	return 0;
}
