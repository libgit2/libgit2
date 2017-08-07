/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_transports_auth_ntlm_h__
#define INCLUDE_transports_auth_ntlm_h__

#include "git2.h"
#include "auth.h"

#ifdef GIT_NTLM

#define NTLMSSP_STAGE_INIT          0
#define NTLMSSP_STAGE_NEGOTIATE     1
#define NTLMSSP_STAGE_CHALLENGE     2
#define NTLMSSP_STAGE_AUTHENTICATE  3

extern int git_http_auth_ntlm(
	git_http_auth_context **out,
	const gitno_connection_data *connection_data);

#else

#define git_http_auth_ntlm git_http_auth_dummy

#endif /* GIT_NTLM */

#endif

