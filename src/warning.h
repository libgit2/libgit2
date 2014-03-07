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

int git_warn(
	git_warning_t warning,
	const char *fmt,
	...);

int git_warn_invalid_data(
	git_warning_t warning,
	const char *data,
	int datalen,
	const char *fmt,
	...);

#endif
