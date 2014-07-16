/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_buf_text_h__
#define INCLUDE_git_buf_text_h__

#include "common.h"
#include "buffer.h"

/**
 * @file git2/buf_text.h
 * @brief Buffer text export structure
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Check quickly if buffer looks like it contains binary data
 *
 * @param buf Buffer to check
 * @return 1 if buffer looks like non-text data
 */
GIT_EXTERN(int) git_buf_text_is_binary(const git_buf *buf);

/**
 * Check quickly if buffer contains a NUL byte
 *
 * @param buf Buffer to check
 * @return 1 if buffer contains a NUL byte
 */
GIT_EXTERN(int) git_buf_text_contains_nul(const git_buf *buf);

GIT_END_DECL

/** @} */

#endif
