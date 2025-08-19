/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "warning.h"

int GIT_CALLBACK(git_warning__callback)(git_warning_t, void *, ...) = NULL;
void *git_warning__data = NULL;

