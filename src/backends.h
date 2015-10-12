/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_backends_h__
#define INCLUDE_backends_h__

#include "git2/common.h"
#include "git2/odb_backend.h"

typedef struct {
	char *name;
	git_odb_backend_ctor ctor;
	void *payload;
} git_odb_registration;

/**
 * Find an ODB registration by name
 */
git_odb_registration *git_odb_backend__find(const char *name);

#endif
