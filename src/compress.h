/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_compress_h__
#define INCLUDE_compress_h__

#include "common.h"

#include "buffer.h"

int git__compress(git_buf *buf, const void *buff, size_t len);

#endif /* INCLUDE_compress_h__ */
