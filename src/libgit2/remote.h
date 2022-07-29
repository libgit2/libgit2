/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_remote_h__
#define INCLUDE_remote_h__

#include "common.h"

#include "git2/remote.h"
#include "git2/transport.h"
#include "git2/sys/remote.h"
#include "git2/sys/transport.h"

#include "refspec.h"
#include "vector.h"
#include "net.h"

#define GIT_REMOTE_ORIGIN "origin"

struct git_remote {
	char *name;
	char *url;
	char *pushurl;
	git_vector refs;
	git_vector refspecs;
	git_vector active_refspecs;
	git_vector passive_refspecs;
	git_vector local_heads;
	git_transport *transport;
	git_repository *repo;
	git_push *push;
	git_indexer_progress stats;
	unsigned int need_pack;
	git_remote_autotag_option_t download_tags;
	int prune_refs;
	int passed_refspecs;
};

int git_remote__urlfordirection(git_str *url_out, struct git_remote *remote, int direction, const git_remote_callbacks *callbacks);
int git_remote__http_proxy(char **out, git_remote *remote, git_net_url *url);

git_refspec *git_remote__matching_refspec(git_remote *remote, const char *refname);
git_refspec *git_remote__matching_dst_refspec(git_remote *remote, const char *refname);

int git_remote__default_branch(git_str *out, git_remote *remote);

int git_remote_connect_options_dup(
	git_remote_connect_options *dst,
	const git_remote_connect_options *src);
int git_remote_connect_options_normalize(
	git_remote_connect_options *dst,
	git_repository *repo,
	const git_remote_connect_options *src);

int git_remote_capabilities(unsigned int *out, git_remote *remote);

#endif
