/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_transports_http_h__
#define INCLUDE_transports_http_h__

#include "settings.h"
#include "httpclient.h"
#include "smart.h"

#define GIT_HTTP_REPLAY_MAX 15

extern bool git_http__expect_continue;

int git_transport_http(git_transport **out, git_remote *owner, void *param);

/*
 * The implementation of HTTP requests for the smart HTTP transport.  Each
 * stream carries a single request and response for a smart protocol service.
 */
typedef struct git_http_backend git_http_backend;

int git_http_backend__new(git_http_backend **out, git_transport_smart_base *owner);
int git_http_backend__open(
	git_transport_smart_stream **out,
	git_http_backend *backend,
	const char *url,
	git_smart_service_t service);
int git_http_backend__close(git_http_backend *backend);
void git_http_backend__free(git_http_backend *backend);

#endif
