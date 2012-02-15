/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_buffer_h__
#define INCLUDE_git_buffer_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/buffer.h
 * @brief Git buffer management routines
 * @defgroup git_buffer Git buffer management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Initialize a git_buf structure.
 *
 * For the cases where GIT_BUF_INIT cannot be used to do static
 * initialization.
 *
 * @param buf pointer to the buffer to be initialized.
 * @param initial_size size the buffer should be initialized to.
 */
GIT_EXTERN(void) git_buf_init(git_buf *buf, size_t initial_size);

/**
 * Free a git_buf structure.
 *
 * @param buf pointer to the buffer to be freed.
 */
GIT_EXTERN(void) git_buf_free(git_buf *buf);

/** @} */
GIT_END_DECL
#endif
