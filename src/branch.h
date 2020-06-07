/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_branch_h__
#define INCLUDE_branch_h__

#include "common.h"

#include "buffer.h"

int git_branch__upstream_name(
	git_buf *buf,
	git_repository *repo,
	const char *refname);

int git_branch__upstream_remote(
	git_buf *buf,
	git_repository *repo,
	const char *refname);

int git_branch__remote_name(
	git_buf *buf,
	git_repository *repo,
	const char *refname);

#endif
