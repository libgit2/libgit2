/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transports_ssh_exec_h__
#define INCLUDE_transports_ssh_exec_h__

#include "common.h"

#include "git2.h"
#include "git2/transport.h"
#include "git2/sys/transport.h"
#include "smart.h"

/* Runs the given git command on the remote; closing the stream ends it. */
int git_ssh_exec__open(
	git_transport_smart_stream **out,
	git_transport_smart_base *transport,
	const char *url,
	const char *cmd);

#endif
