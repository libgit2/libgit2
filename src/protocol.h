/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_protocol_h__
#define INCLUDE_protocol_h__

#include "transport.h"
#include "buffer.h"
#include "pkt.h"

int git_protocol_store_refs(git_transport *t, int flushes);
int git_protocol_detect_caps(git_pkt_ref *pkt, git_transport_caps *caps);

#define GIT_SIDE_BAND_DATA     1
#define GIT_SIDE_BAND_PROGRESS 2
#define GIT_SIDE_BAND_ERROR    3

#endif
