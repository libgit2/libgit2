/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_warning_h__
#define INCLUDE_warning_h__

#include "common.h"
#include "git2/warning.h"

/**
 * Raise a warning.
 *
 * It returns the return value from the user-specified callback. If no
 * callback is set, it returns 0.
 */
extern int git_warning__raise(git_warning *warning);

#endif
