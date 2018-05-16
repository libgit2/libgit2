/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_path_h__
#define INCLUDE_sys_git_path_h__

#include "git2/common.h"
#include "git2/types.h"

GIT_BEGIN_DECL

/**
 * Check whether a path component corresponds to a .gitmodules file
 *
 * @param name the path component to check
 */
GIT_EXTERN(int) git_path_is_dotgit_modules(const char *name);

/**
 * Check whether a path component corresponds to a .gitignore file
 *
 * @param name the path component to check
 */
GIT_EXTERN(int) git_path_is_dotgit_ignore(const char *name);

/**
 * Check whether a path component corresponds to a .gitignore file
 *
 * @param name the path component to check
 */
GIT_EXTERN(int) git_path_is_dotgit_attributes(const char *name);

GIT_END_DECL
#endif
