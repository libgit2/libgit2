/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_varint_h__
#define INCLUDE_varint_h__

#include <stdint.h>

extern int encode_varint(uintmax_t, unsigned char *);
extern uintmax_t decode_varint(const unsigned char **);

#endif
