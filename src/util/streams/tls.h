/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_streams_tls_h__
#define INCLUDE_streams_tls_h__

#include "git2_util.h"

#include "git2/sys/stream.h"

/**
 * Create a TLS stream with the most appropriate backend available for
 * the current platform, whether that's SecureTransport on macOS,
 * OpenSSL or mbedTLS on other Unixes, or something else entirely.
 */
extern int git_stream_tls_new(git_stream **out);

#endif
