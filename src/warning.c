/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "warning.h"

static void *warning_payload;
static git_warning_cb warning_callback;

int git_warning_set_callback(git_warning_cb callback, void *payload)
{
	warning_callback = callback;
	warning_payload = callback ? payload : NULL;

	return 0;
}

int git_warning__raise(git_warning *warning)
{
	if (!warning_callback)
		return 0;

	return warning_callback(warning, warning_payload);
}
