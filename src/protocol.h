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

typedef struct {
	git_transport *transport;
	git_vector *refs;
	git_buf buf;
	int error;
	unsigned int flush :1;
} git_protocol;

int git_protocol_store_refs(git_protocol *p, const char *data, size_t len);

#endif
