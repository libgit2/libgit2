/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_generic_h__
#define INCLUDE_hash_generic_h__

#include "hash.h"

struct git_hash_ctx {
    unsigned long long size;
    unsigned int H[5];
    unsigned int W[16];
};

#endif /* INCLUDE_hash_generic_h__ */
