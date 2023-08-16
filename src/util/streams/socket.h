/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_streams_socket_h__
#define INCLUDE_streams_socket_h__

#include "git2_util.h"

#include "stream.h"

extern int git_stream_socket_new(git_stream **out, const char *host, const char *port);

extern int git_stream_socket_global_init(void);

#endif
