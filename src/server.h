/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_server_h__
#define INCLUDE_server_h__

#include "common.h"
#include "transports/smart.h"
#include "array.h"

typedef git_array_t(git_oid) git_oid_array;

struct git_server {
	enum git_request_type type;
	git_repository *repo;
	gitno_socket s;
	int rpc;
	char *path;
	git_oid_array common;
	git_oid_array wants;
};

extern int git_server_new(git_server **out, git_repository *repo, int fd);
extern void git_server_free(git_server *server);
extern int git_server__handle_request(git_server *server, git_pkt *pkt);
extern int git_server__ls(git_buf *out, git_server *server);
extern int git_server__negotiation(git_server *server, git_pkt *_pkt);

#endif

