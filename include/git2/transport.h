/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_transport_h__
#define INCLUDE_git_transport_h__

#include "indexer.h"
#include "net.h"
#include "types.h"
#include "cert.h"
#include "credential.h"

/**
 * @file git2/transport.h
 * @brief Transports are the low-level mechanism to connect to a remote server
 * @defgroup git_transport Transports are the low-level mechanism to connect to a remote server
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Callback for messages received by the transport.
 *
 * Return a negative value to cancel the network operation.
 *
 * @param str The message from the transport
 * @param len The length of the message
 * @param payload Payload provided by the caller
 * @return 0 on success or an error code
 */
typedef int GIT_CALLBACK(git_transport_message_cb)(const char *str, int len, void *payload);

/**
 * Signature of a function which creates a transport.
 *
 * @param out the transport generate
 * @param owner the owner for the transport
 * @param param the param to the transport creation
 * @return 0 on success or an error code
 */
typedef int GIT_CALLBACK(git_transport_cb)(git_transport **out, git_remote *owner, void *param);

/** @} */
GIT_END_DECL

#endif
