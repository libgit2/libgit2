/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_warning_h__
#define INCLUDE_warning_h__

#include "common.h"
#include "git2/sys/warning.h"

extern void git_warning_set_callback(git_warning_callback cb, void *payload);

extern int git_warning(
	git_error_t klass,
	git_repository *repo,
	git_otype otype,
	const void *object,
	const char *fmt,
	...);

#endif
