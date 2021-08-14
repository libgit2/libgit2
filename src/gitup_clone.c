/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "clone.h"

#include "git2/clone.h"
#include "git2/remote.h"
#include "git2/repository.h"
#include "git2/config.h"
#include "git2/checkout.h"

#include "remote.h"
#include "futils.h"
#include "refs.h"
#include "repository.h"
#include "odb.h"

GIT_EXTERN(int) gitup_clone_into(
	git_repository **out,
	const char *url,
	const char *local_path,
	const git_clone_options *options
	) {
	return git_clone(out, url, local_path, options);
}

GIT_EXTERN(int) gitup_clone_into_old(
	git_repository *repo,
	git_remote *remote,
	const git_fetch_options *fetch_opts,
	const git_checkout_options *checkout_opts,
	const char *branch) {
	git_clone_options options = GIT_CLONE_OPTIONS_INIT;
	const char *url;
	const char *local_path;
	/// We have to create git_clone_options
	assert(repo && remote && fetch_opts && checkout_opts);

	options.checkout_opts = *checkout_opts;
	options.fetch_opts = *fetch_opts;
	options.checkout_branch = branch;
	url = git_remote_url(remote);
	local_path = git_repository_path(repo);
	return gitup_clone_into(&repo, url, local_path, &options);
}
