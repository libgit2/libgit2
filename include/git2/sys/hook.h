/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_hook_h__
#define INCLUDE_sys_git_hook_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"

/**
 * @file git2/sys/hook.h
 * @brief Low-level Git hook calls
 * @defgroup git_hook_call Low-level Git hook calls
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * A helper to get the path of the repository's hooks.
 *
 * This will obey core.hooksPath.
 *
 * @param out_dir The absolute path to the hooks location.
 * @param repo The repository to get the hooks location from.
 * @return 0 on success, or an error code.
 */
GIT_EXTERN(int) git_hook_dir(git_buf *out_dir, git_repository *repo);

/** @} */
GIT_END_DECL

#endif
