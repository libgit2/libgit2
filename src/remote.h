/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_remote_h__
#define INCLUDE_remote_h__

#include "git2/remote.h"

#include "refspec.h"
#include "transport.h"
#include "repository.h"

#define GIT_REMOTE_ORIGIN "origin"

struct git_remote {
	char *name;
	char *url;
	char *pushurl;
	git_vector refs;
	struct git_refspec fetch;
	struct git_refspec push;
	git_transport *transport;
	git_repository *repo;
	git_remote_callbacks callbacks;
	unsigned int need_pack:1,
		download_tags:2, /* There are four possible values */
		check_cert:1;
};

const char* git_remote__urlfordirection(struct git_remote *remote, int direction);

#endif
