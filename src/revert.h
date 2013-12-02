/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_revert_h__
#define INCLUDE_revert_h__

#include "git2/repository.h"

int git_revert__cleanup(git_repository *repo);

#endif
