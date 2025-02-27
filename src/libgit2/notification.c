/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "notification.h"

int GIT_CALLBACK(git_notification__callback)(
	git_notification_level_t,
	git_notification_t,
	const char *,
	void *,
	...) = NULL;

void *git_notification__data = NULL;
