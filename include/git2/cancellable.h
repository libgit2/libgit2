/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_cancelable_h__
#define INCLUDE_git_cancelable_h__

#include "common.h"
#include "types.h"

typedef struct git_cancellable git_cancellable;
typedef struct git_cancellable_source git_cancellable_source;

typedef int (*git_cancellable_cb)(git_cancellable *cancellable, void *payload);

GIT_EXTERN(int) git_cancellable_source_new(git_cancellable_source **out);
GIT_EXTERN(void) git_cancellable_source_free(git_cancellable_source *cs);
GIT_EXTERN(int) git_cancellable_is_cancelled(git_cancellable *c);
GIT_EXTERN(int) git_cancellable_register(git_cancellable *c, git_cancellable_cb cb, void *payload);
GIT_EXTERN(git_cancellable *) git_cancellable_source_token(git_cancellable_source *cs);
GIT_EXTERN(int) git_cancellable_source_cancel(git_cancellable_source *cs);

#endif
