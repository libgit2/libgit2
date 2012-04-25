/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_revparse_h__
#define INCLUDE_git_revparse_h__

#include "common.h"
#include "types.h"


GIT_BEGIN_DECL

GIT_EXTERN(int) git_revparse_single(git_object **out, git_repository *repo, const char *spec);

//GIT_EXTERN(int) git_revparse_multi(TODO);

GIT_END_DECL

#endif
