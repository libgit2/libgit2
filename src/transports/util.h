/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transport_util_h__
#define INCLUDE_transport_util_h__

#include "vector.h"
#include "transport.h"
#include "netops.h"
#include "protocol.h"

int detect_caps(git_transport_caps *caps, git_vector *refs);
int store_refs(git_protocol *proto, gitno_buffer *buf);

#endif
