/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_transports_auth_default_h__
#define INCLUDE_transports_auth_default_h__

#include "common.h"
#include "git2.h"

#ifdef GIT_WIN32

/*
 * Provide the default credentials of the logged-on user when the
 * server supports them and the URL is in the Local Machine, Intranet
 * or Trusted zone.  Returns 0 when `cred` was set, 1 when no default
 * credentials apply, or an error code.
 */
extern int git_http_auth_default_fallback(
	git_credential **cred,
	const char *url,
	unsigned int allowed_types);

#endif

#endif
