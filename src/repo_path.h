/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_repo_path_h__
#define INCLUDE_repo_path_h__

#include "common.h"

/* Flags to determine path validity in `git_path_isvalid` */
#define GIT_PATH_REJECT_TRAVERSAL          (1 << 0)
#define GIT_PATH_REJECT_DOT_GIT            (1 << 1)
#define GIT_PATH_REJECT_SLASH              (1 << 2)
#define GIT_PATH_REJECT_BACKSLASH          (1 << 3)
#define GIT_PATH_REJECT_TRAILING_DOT       (1 << 4)
#define GIT_PATH_REJECT_TRAILING_SPACE     (1 << 5)
#define GIT_PATH_REJECT_TRAILING_COLON     (1 << 6)
#define GIT_PATH_REJECT_DOS_PATHS          (1 << 7)
#define GIT_PATH_REJECT_NT_CHARS           (1 << 8)
#define GIT_PATH_REJECT_DOT_GIT_LITERAL    (1 << 9)
#define GIT_PATH_REJECT_DOT_GIT_HFS        (1 << 10)
#define GIT_PATH_REJECT_DOT_GIT_NTFS       (1 << 11)

/* Default path safety for writing files to disk: since we use the
 * Win32 "File Namespace" APIs ("\\?\") we need to protect from
 * paths that the normal Win32 APIs would not write.
 */
#ifdef GIT_WIN32
# define GIT_PATH_REJECT_FILESYSTEM_DEFAULTS \
	GIT_PATH_REJECT_TRAVERSAL | \
	GIT_PATH_REJECT_BACKSLASH | \
	GIT_PATH_REJECT_TRAILING_DOT | \
	GIT_PATH_REJECT_TRAILING_SPACE | \
	GIT_PATH_REJECT_TRAILING_COLON | \
	GIT_PATH_REJECT_DOS_PATHS | \
	GIT_PATH_REJECT_NT_CHARS
#else
# define GIT_PATH_REJECT_FILESYSTEM_DEFAULTS \
	GIT_PATH_REJECT_TRAVERSAL
#endif

 /* Paths that should never be written into the working directory. */
#define GIT_PATH_REJECT_WORKDIR_DEFAULTS \
	GIT_PATH_REJECT_FILESYSTEM_DEFAULTS | GIT_PATH_REJECT_DOT_GIT

/* Paths that should never be written to the index. */
#define GIT_PATH_REJECT_INDEX_DEFAULTS \
	GIT_PATH_REJECT_TRAVERSAL | GIT_PATH_REJECT_DOT_GIT

/*
 * Determine whether a path is a valid git path or not - this must not contain
 * a '.' or '..' component, or a component that is ".git" (in any case).
 *
 * `repo` is optional.  If specified, it will be used to determine the short
 * path name to reject (if `GIT_PATH_REJECT_DOS_SHORTNAME` is specified),
 * in addition to the default of "git~1".
 */
extern bool git_path_isvalid(
	git_repository *repo,
	const char *path,
	uint16_t mode,
	unsigned int flags);

#endif
