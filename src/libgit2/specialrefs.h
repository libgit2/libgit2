/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_specialrefs_h__
#define INCLUDE_specialrefs_h__

#include "common.h"

#include "git2/refs.h"

extern int git_specialref_lookup_head(git_reference **out, git_repository *repo);

#endif
