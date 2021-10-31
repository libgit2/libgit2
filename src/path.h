/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_path_h__
#define INCLUDE_path_h__

#include "common.h"

#include "fs_path.h"
#include <git2/sys/path.h>

#define GIT_PATH_REJECT_DOT_GIT            (GIT_FS_PATH_REJECT_MAX << 1)
#define GIT_PATH_REJECT_DOT_GIT_LITERAL    (GIT_FS_PATH_REJECT_MAX << 2)
#define GIT_PATH_REJECT_DOT_GIT_HFS        (GIT_FS_PATH_REJECT_MAX << 3)
#define GIT_PATH_REJECT_DOT_GIT_NTFS       (GIT_FS_PATH_REJECT_MAX << 4)

/* Paths that should never be written into the working directory. */
#define GIT_PATH_REJECT_WORKDIR_DEFAULTS \
	GIT_FS_PATH_REJECT_FILESYSTEM_DEFAULTS | GIT_PATH_REJECT_DOT_GIT

/* Paths that should never be written to the index. */
#define GIT_PATH_REJECT_INDEX_DEFAULTS \
	GIT_FS_PATH_REJECT_TRAVERSAL | GIT_PATH_REJECT_DOT_GIT

extern bool git_path_validate(
	git_repository *repo,
	const char *path,
	uint16_t file_mode,
	unsigned int flags);

#endif
