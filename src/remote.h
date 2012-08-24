/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_remote_h__
#define INCLUDE_remote_h__

#include "refspec.h"
#include "transport.h"
#include "repository.h"

struct git_remote {
	char *name;
	char *url;
	git_vector refs;
	struct git_refspec fetch;
	struct git_refspec push;
	git_transport *transport;
	git_repository *repo;
	unsigned int need_pack:1;
};

#endif
