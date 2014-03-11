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

/**
 * Use this to raise a warning
 *
 * @param warning A git_warning_t code from include/git2/sys/warning.h
 * @param default_rval Default return value (i.e. error code or zero)
 * @param fmt Printf-style format string for warning message
 * @return 0 to continue, less than 0 to raise error
 */
int git_warn(
	git_warning_t warning,
	int default_rval,
	const char *fmt,
	...);

/**
 * Raise a warning about invalid data, via a git_warning_invalid_data struct
 */
int git_warn_invalid_data(
	git_warning_t warning,
	int default_rval,
	const char *data,
	int datalen,
	const char *fmt,
	...);

#endif
