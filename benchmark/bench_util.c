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
	git_buf buf_env = GIT_BUF_INIT;
	git_buf tempdir = GIT_BUF_INIT;
	static const char *tempname = "libgit2_bench_XXXXXX";
	int error = -1;

	if ((git_buf_sets(&buf_env, getenv("BM_TEMP")) < 0) ||
		(git_buf_len(&buf_env) == 0)) {
		giterr_set(GITERR_OS, "Set BM_TEMP to tempdir");
		error = -1;
		goto done;
	}

#ifdef GIT_WIN32
	git_path_mkposix(buf_env.ptr);
#endif

	if ((!tempdir_isvalid(buf_env.ptr)) ||
		(git_buf_joinpath(&tempdir, buf_env.ptr, tempname) < 0)) {
		giterr_set(GITERR_OS, "Set BM_TEMP to tempdir");
		error = -1;
		goto done;
	}

#ifdef GIT_WIN32
	if (_mktemp_s(tempdir.ptr, tempdir.size+1) != 0 ||
		p_mkdir(tempdir.ptr, 0700) != 0) {
		giterr_set(GITERR_OS, "Set BM_TEMP to tempdir");
		error = -1;
		goto done;
	}
#else
	if (mkdtemp(tempdir.ptr) == NULL) {
		giterr_set(GITERR_OS, "Set BM_TEMP to tempdir");
		error = -1;
		goto done;
	}
#endif

	*out = git_buf_detach(&tempdir);
	error = 0;

done:
	git_buf_free(&buf_env);
	git_buf_free(&tempdir);
	return error;
}
