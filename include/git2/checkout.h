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


#define GIT_CHECKOUT_OVERWRITE_EXISTING 0
#define GIT_CHECKOUT_SKIP_EXISTING 1


typedef struct git_checkout_opts {
	git_indexer_stats stats;
	int existing_file_action;
	int apply_filters;
	int dir_mode;
	int file_open_mode;
} git_checkout_opts;

#define GIT_CHECKOUT_DEFAULT_OPTS {  \
	{0},                              \
	GIT_CHECKOUT_OVERWRITE_EXISTING,  \
	true,                             \
	GIT_DIR_MODE,                     \
	O_CREAT|O_TRUNC|O_WRONLY          \
}

/**
 * Updates files in the working tree to match the index.
 *
 * @param repo repository to check out (must be non-bare)
 * @param opts specifies checkout options (may be NULL)
 * @return 0 on success, GIT_ERROR otherwise (use git_error_last for information about the error)
 */
GIT_EXTERN(int) git_checkout_index(git_repository *repo, git_checkout_opts *opts);

/**
 * Updates files in the working tree to match the commit pointed to by HEAD.
 *
 * @param repo repository to check out (must be non-bare)
 * @param opts specifies checkout options (may be NULL)
 * @return 0 on success, GIT_ERROR otherwise (use git_error_last for information about the error)
 */
GIT_EXTERN(int) git_checkout_head(git_repository *repo, git_checkout_opts *opts);

/** @} */
GIT_END_DECL
#endif
