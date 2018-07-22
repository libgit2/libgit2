/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_warning_h__
#define INCLUDE_warning_h__

#include "common.h"
#include "git2/warning.h"
#include "vector.h"

/*
 * ask for the user for a resolution
 * GIT_PASSTHROUGH means the user doesn't care, so do "the right thing"
 * a negative return would be considered an error in the user's code.
 * a positive return would be given meaning depending on the warning class.
 */
int git_warn__raise(git_warning_class klass, git_repository *repo, void *context, const char *msg, ...);

int git_warning_global_init(void);
void git_warning_global_shutdown(void);

extern git_vector git_warning__registration;

#endif
