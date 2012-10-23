/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_merge_h__
#define INCLUDE_merge_h__

#include "git2/types.h"

#define ORIG_HEAD_FILE			"ORIG_HEAD"
#define MERGE_HEAD_FILE			"MERGE_HEAD"
#define MERGE_MSG_FILE			"MERGE_MSG"
#define MERGE_MODE_FILE			"MERGE_MODE"

#define MERGE_CONFIG_FILE_MODE	0666

int git_merge__cleanup(git_repository *repo);

#endif
