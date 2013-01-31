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
		const char *user_from_url,
		unsigned int allowed_types,
		void *payload)
{
	git_cred_userpass_payload *userpass = (git_cred_userpass_payload*)payload;
	const char *effective_username = NULL;

	GIT_UNUSED(url);

	if (!userpass || !userpass->password) return -1;

	/* Username resolution: a username can be passed with the URL, the
	 * credentials payload, or both. Here's what we do.
	 *
	 * |  Payload    |   URL    |   Used    |
	 * +-------------+----------+-----------+
	 * |    yes      |   no     |  payload  |
	 * |    yes      |   yes    |  payload  |
	 * |    no       |   yes    |  url      |
	 * |    no       |   no     |  FAIL     |
	 */
	effective_username = userpass->username;
	if (!userpass->username && user_from_url)
		effective_username = user_from_url;
	if (!effective_username) return -1;

	if ((GIT_CREDTYPE_USERPASS_PLAINTEXT & allowed_types) == 0 ||
			git_cred_userpass_plaintext_new(cred, effective_username, userpass->password) < 0)
		return -1;

	return 0;
}
