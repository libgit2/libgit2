/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_base64_h__
#define INCLUDE_base64_h__

#include "common.h"

int git_base64_encode(char *out, size_t outlen, const char *in, size_t inlen);

#endif /* INCLUDE_base64_h__ */
