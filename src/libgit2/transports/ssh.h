/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_transports_ssh_h__
#define INCLUDE_transports_ssh_h__

const char *git_ssh__backend_name(void);
int git_ssh__set_backend(const char *name);
int git_transport_ssh_global_init(void);

#endif
