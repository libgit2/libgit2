/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_remote_h__
#define INCLUDE_remote_h__

#include "git2/remote.h"
#include "git2/transport.h"

#include "refspec.h"
#include "repository.h"

#define GIT_REMOTE_ORIGIN "origin"

struct git_remote {
	char *name;
	char *url;
	char *pushurl;
	git_vector refs;
	struct git_refspec fetch;
	struct git_refspec push;
	git_cred_acquire_cb cred_acquire_cb;
	void *cred_acquire_payload;
	git_transport *transport;
	git_repository *repo;
	git_remote_callbacks callbacks;
	git_transfer_progress stats;
	unsigned int need_pack;
	git_remote_autotag_option_t download_tags;
	unsigned int check_cert;
	unsigned int update_fetchhead;
};

const char* git_remote__urlfordirection(struct git_remote *remote, int direction);
int git_remote__get_http_proxy(git_remote *remote, bool use_ssl, char **proxy_url);

#endif
