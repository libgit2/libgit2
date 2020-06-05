/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#include "git2_util.h"
#include "errors.h"
#include "thread-utils.h"
#include "integer.h"

/*
 * Include the declarations for deprecated functions; this ensures
 * that they're decorated with the proper extern/visibility attributes.
 *
 * Before doing that, declare that we don't want compatibility git_buf
 * definitions.  We want to avoid intermingling the public compatibility
 * layer with our actual git_buf types and functions.
 */

#define GIT_DEPRECATE_BUF
#include "git2/deprecated.h"

#endif
