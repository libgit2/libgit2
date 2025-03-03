/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_notification_h__
#define INCLUDE_git_notification_h__

#include "common.h"

/**
 * @file git2/notification.h
 * @brief Git notification routines
 * @defgroup git_notification Git notification routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * The notification level. Most of these notifications are "informational";
 * by default, the notification levels below `GIT_NOTIFICATION_FATAL` will
 * be raised but continue program execution. For these informational
 * notifications, an application _may_ decide to stop processing (by
 * returning a non-zero code from the notification callback). An example of
 * an informational notification is a line ending misconfiguration when
 * `core.safecrlf=warn` is configured.
 *
 * However, the notification `GIT_NOTIFICATION_FATAL` has different
 * behavior; these notifications are raised before libgit2 stops processing
 * and gives callers the ability to continue anyway.
 */
typedef enum {
	/**
	 * An informational message; by default, libgit2 will continue
	 * function execution.
	 */
	GIT_NOTIFICATION_INFO = 0,

	/**
	 * A warning; by default, libgit2 will continue function execution
	 * and will not return an error code. A notification callback can
	 * override this behavior and cause libgit2 to return immediately.
	 *
	 * For example, when line-ending issues are encountered and
	 * `core.safecrlf=warn`, a warning notification is raised, but
	 * function execution otherwise continues.
	 */
	GIT_NOTIFICATION_WARN = 1,

	/**
	 * An error where, by default, libgit2 would continue function
	 * execution but return an error code at the end of execution.
	 * A notification callback can override this behavior and cause
	 * libgit2 to return immediately.
	 *
	 * For example, during checkout, non-fatal errors may be raised
	 * while trying to write an individual file (perhaps due to
	 * platform filename limitations). In this case, an error-level
	 * notification will be raised, checkout will continue to put files
	 * on disk, but the function will return an error code upon
	 * completion.
	 */
	GIT_NOTIFICATION_ERROR = 2,

	/**
	 * A severe error where, by default, libgit2 would stop function
	 * execution immediately and return an error code. A caller may
	 * wish to get additional insight into the error in the structured
	 * notification content.
	 *
	 * For example, a `safe.directory` is a fatal error.
	 */
	GIT_NOTIFICATION_FATAL = 3
} git_notification_level_t;

/**
 * The notification type. Any notification that is sent by libgit2 will
 * be a unique type, potentially with detailed information about the
 * state of the notification.
 */
typedef enum {
	/**
	 * A notification provided when `core.safecrlf` is configured and a
	 * file has line-ending reversability problems. The level will be
	 * `WARN` (when `core.safecrlf=warn`) or `FATAL` (when
	 * `core.safecrlf=on`).
	 *
	 * The data will be:
	 *
	 * - `const char *path`: the path to the file
	 * - `const char *message`: the notification message
	 */
	GIT_NOTIFICATION_CRLF = 1
} git_notification_t;

/** @} */
GIT_END_DECL
#endif
