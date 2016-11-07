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
 * @brief High-level Git hook calls
 * @defgroup git_hook_call High-level Git hook calls
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

GIT_EXTERN(int) git_hook_call_pre_rebase(git_repository *repo, const git_annotated_commit *upstream, const git_annotated_commit *rebased);

/** @} */
GIT_END_DECL

#endif
