/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transports_smartnew_h__
#define INCLUDE_transports_smartnew_h__

#include "common.h"

typedef struct {
} git_smart;

extern int git_smart_init(git_smart *smart);
extern int git_smart_close(git_smart *smart);
extern void git_smart_dispose(git_smart *smart);

#endif
