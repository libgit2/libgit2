/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_notification_h__
#define INCLUDE_notification_h__

#include "common.h"
#include "git2/notification.h"

extern int GIT_CALLBACK(git_notification__callback)(
	git_notification_level_t,
	git_notification_t,
	const char *,
	void *,
	...);
extern void *git_notification__data;

#define git_notification(level, notification, message, ...) \
	((git_notification__callback == NULL) ? 0 : \
	 git_notification__callback(level, notification, message, \
	                            git_notification__data, __VA_ARGS__))

#endif
