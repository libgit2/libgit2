/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2/cred_helpers.h"

int git_cred_userpass(
		git_cred **cred,
		const char *url,
		unsigned int allowed_types,
		void *payload)
{
	git_cred_userpass_payload *userpass = (git_cred_userpass_payload*)payload;

	GIT_UNUSED(url);

	if (!userpass || !userpass->username || !userpass->password) return -1;

	if ((GIT_CREDTYPE_USERPASS_PLAINTEXT & allowed_types) == 0 ||
			git_cred_userpass_plaintext_new(cred, userpass->username, userpass->password) < 0)
		return -1;

	return 0;
}
