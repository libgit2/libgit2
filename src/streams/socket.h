/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_streams_socket_h__
#define INCLUDE_streams_socket_h__

#include "common.h"

#include "netops.h"

typedef struct {
	git_stream parent;
	char *host;
	char *port;
	git_socket s;
	
	git_buf sock_buf;
	git_remote_events_cb fd_events_cb;
	void *payload;
} git_socket_stream;

extern int git_socket_stream_new(git_stream **out, const char *host, const char *port, git_remote_events_cb fd_events_cb, void *payload);

#endif
