/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_sys_git_remote_h
#define INCLUDE_sys_git_remote_h

/**
 * @file git2/sys/remote.h
 * @brief Low-level remote functionality for custom transports
 * @defgroup git_remote Low-level remote functionality
 * @ingroup Git
 * @{
*/

GIT_BEGIN_DECL

typedef enum {
	/** Remote supports fetching an advertised object by ID. */
	GIT_REMOTE_CAPABILITY_TIP_OID = (1 << 0),

	/** Remote supports fetching an individual reachable object. */
	GIT_REMOTE_CAPABILITY_REACHABLE_OID = (1 << 1),
} git_remote_capability_t;

/** @} */
GIT_END_DECL
#endif
