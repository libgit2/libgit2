/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_checkout_h__
#define INCLUDE_git_checkout_h__

#include "common.h"
#include "types.h"
#include "indexer.h"


/**
 * @file git2/checkout.h
 * @brief Git checkout routines
 * @defgroup git_checkout Git checkout routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Updates files in the working tree to match the version in the index.
 *
 * @param repo repository to check out (must be non-bare)
 * @param stats pointer to structure that receives progress information (may be NULL)
 * @return 0 on success, GIT_ERROR otherwise (use git_error_last for information about the error)
 */
GIT_EXTERN(int) git_checkout_force(git_repository *repo, git_indexer_stats *stats);

/** @} */
GIT_END_DECL
#endif
