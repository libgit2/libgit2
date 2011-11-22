/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_transport_h__
#define INCLUDE_git_transport_h__

#include "common.h"
#include "types.h"
#include "net.h"

/**
 * @file git2/transport.h
 * @brief Git protocol transport abstraction
 * @defgroup git_transport Git protocol transport abstraction
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Get the appropriate transport for an URL.
 * @param tranport the transport for the url
 * @param url the url of the repo
 */
GIT_EXTERN(int) git_transport_new(git_transport **transport, const char *url);

/**
 * Return whether a string is a valid transport URL
 *
 * @param tranport the url to check
 * @param 1 if the url is valid, 0 otherwise
 */
GIT_EXTERN(int) git_transport_valid_url(const char *url);

/** @} */
GIT_END_DECL
#endif
