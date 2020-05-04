/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_git_ssh_subtransport_h__
#define INCLUDE_git_ssh_subtransport_h__

#include <git2/sys/transport.h>

/**
 * @file git2client/ssh_subtransport.h
 * @brief Git client transport to interact with the ssh command-line interface
 * @defgroup git_ssh_subtransport Git ssh client transport registration
 * @ingroup Git
 * @{
 */

GIT_BEGIN_DECL

/** An ssh command-line interface subtransport. */
typedef struct git_ssh_subtransport git_ssh_subtransport;

/** An ssh command-line subtransport stream. */
typedef struct git_ssh_subtransport_stream git_ssh_subtransport_stream;

/**
 * Creates a new ssh subtransport that will use the `ssh` command-line
 * application to communicate.  This subtransport can be registered with
 * `git_transport_register`.
 *
 * @see git_transport_register
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_ssh_subtransport_new(
	git_smart_subtransport **out,
	git_transport *owner,
	void *payload);

/**
 * Registers a custom ssh subtransport with libgit2; any `ssh://` protocol
 * remotes will use the `ssh` command-line application to communicate.
 *
 * If libgit2 was build with internal ssh support, this will override that.
 *
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_ssh_subtransport_register(void);

/** @} */
GIT_END_DECL
#endif
