/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_filediff_h__
#define INCLUDE_filediff_h__

/* xdiff cannot cope with large files, just treat them as binary */
#define GIT_MERGE_FILE_XDIFF_MAX (1024UL * 1024 * 1023)

#endif
