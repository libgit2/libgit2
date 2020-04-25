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
#include "git2/sys/transport.h"

#include "refspec.h"
#include "vector.h"

#define GIT_REMOTE_ORIGIN "origin"

typedef struct git_remote_connection_opts {
	const git_strarray *custom_headers;
	const git_proxy_options *proxy;
	git_direction dir;
} git_remote_connection_opts;

#define GIT_REMOTE_CONNECTION_OPTIONS_INIT { NULL, NULL, GIT_DIRECTION_FETCH }

typedef int GIT_CALLBACK(git_perform_cb)(git_remote *remote, git_event_t events);

#define GIT_REMOTE_NUM_PERFORMCB	3U

struct git_remote {
	char *name;
	char *url;
	char *pushurl;
	git_vector refs;
	git_vector refspecs;
	git_vector active_refspecs;
	git_vector passive_refspecs;
	git_transport *transport;
	git_repository *repo;
	git_push *push;
	git_indexer_progress stats;
	unsigned int need_pack;
	git_remote_autotag_option_t download_tags;
	int prune_refs;
	int passed_refspecs;
	
	git_perform_cb perform_callbacks[GIT_REMOTE_NUM_PERFORMCB];
	size_t perform_num_cb;

	git_buf resolved_url;
	git_transport *connect_transport;
	
	git_strarray custom_headers;
	git_proxy_options proxy_options;
	git_direction dir;

	union
	{
		git_fetch_options fetch;
		git_push_options push;
	} opts;

	git_strarray requested_refspecs;
	git_buf reflog_message;

	void *cbref;
	git_remote_callbacks callbacks;
};

int git_remote__connect(git_remote *remote);

int git_remote__urlfordirection(git_buf *url_out, struct git_remote *remote, int direction, const git_remote_callbacks *callbacks);
int git_remote__get_http_proxy(git_remote *remote, bool use_ssl, char **proxy_url);

git_refspec *git_remote__matching_refspec(git_remote *remote, const char *refname);
git_refspec *git_remote__matching_dst_refspec(git_remote *remote, const char *refname);

#endif
