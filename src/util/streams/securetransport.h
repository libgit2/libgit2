/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_streams_securetransport_h__
#define INCLUDE_streams_securetransport_h__

#include "git2_util.h"

#include "git2/sys/stream.h"

#ifdef GIT_HTTPS_SECURETRANSPORT

extern int git_stream_securetransport_new(git_stream **out, const char *host, const char *port);
extern int git_stream_securetransport_wrap(git_stream **out, git_stream *in, const char *host);

#endif

#endif
