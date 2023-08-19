/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transport_h__
#define INCLUDE_transport_h__

#include "common.h"
#include "git2/sys/stream.h"

extern int git_transport__timeout;
extern int git_transport__connect_timeout;

GIT_INLINE(void) git_transport__set_connect_opts(git_stream_connect_options *opts)
{
	opts->timeout = git_transport__timeout;
	opts->connect_timeout = git_transport__connect_timeout;
}

#endif
