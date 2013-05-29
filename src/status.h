/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_status_h__
#define INCLUDE_status_h__

#include "git2/types.h"

struct git_status_iterator {
	const git_status_options *opts;

	git_diff_list *h2i;
	size_t h2i_idx;
	size_t h2i_len;

	git_diff_list *i2w;
	size_t i2w_idx;
	size_t i2w_len;

	int (*strcomp)(const char *, const char *);
};

#endif /* INCLUDE_status_h__ */
