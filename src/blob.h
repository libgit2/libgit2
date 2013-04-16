/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_blob_h__
#define INCLUDE_blob_h__

#include "git2/blob.h"
#include "repository.h"
#include "odb.h"
#include "fileops.h"

struct git_blob {
	git_object object;
	git_odb_object *odb_object;
};

void git_blob__free(void *blob);
int git_blob__parse(void *blob, git_odb_object *obj);
int git_blob__getbuf(git_buf *buffer, git_blob *blob);

#endif
