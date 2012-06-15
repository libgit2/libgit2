/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_clone_h__
#define INCLUDE_git_clone_h__

#include "common.h"
#include "types.h"


/**
 * @file git2/clone.h
 * @brief Git cloning routines
 * @defgroup git_clone Git cloning routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * TODO
 *
 * @param out pointer that will receive the resulting repository object
 * @param origin_url repository to clone from
 * @param dest_path local directory to clone to
 * @return 0 on success, GIT_ERROR otherwise (use git_error_last for information about the error)
 */
GIT_EXTERN(int) git_clone(git_repository **out, const char *origin_url, const char *dest_path);

/**
 * TODO
 *
 * @param out pointer that will receive the resulting repository object
 * @param origin_url repository to clone from
 * @param dest_path local directory to clone to
 * @return 0 on success, GIT_ERROR otherwise (use git_error_last for information about the error)
 */
GIT_EXTERN(int) git_clone_bare(git_repository **out, const char *origin_url, const char *dest_path);

/** @} */
GIT_END_DECL
#endif
