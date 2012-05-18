/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_fetch_h__
#define INCLUDE_fetch_h__

#include "netops.h"

int git_fetch_negotiate(git_remote *remote);
int git_fetch_download_pack(git_remote *remote, git_off_t *bytes, git_indexer_stats *stats);

int git_fetch__download_pack(const char *buffered, size_t buffered_size, GIT_SOCKET fd,
			     git_repository *repo, git_off_t *bytes, git_indexer_stats *stats);
int git_fetch_setup_walk(git_revwalk **out, git_repository *repo);

#endif
