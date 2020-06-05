/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_git_exec_filter_h__
#define INCLUDE_git_exec_filter_h__

/**
 * @file git2client/exec_filter.h
 * @brief Git filter that will execute the specified command-line
 * @defgroup git_exec_filter Git exec filter registration
 * @ingroup Git
 * @{
 */

GIT_BEGIN_DECL

/**
 * Registers a custom filter driver that will execute the command-line
 * application specified in the attributes.
 *
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_exec_filter_register(void);

/** @} */
GIT_END_DECL
#endif /* INCLUDE_git_exec_filter_h__ */
