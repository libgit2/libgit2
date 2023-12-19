/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sparse_h__
#define INCLUDE_sparse_h__

#include "common.h"

#include "repository.h"
#include "attr_file.h"

#define GIT_SPARSE_CHECKOUT_FILE "sparse-checkout"
typedef struct {
	git_repository *repo;
	git_attr_file *sparse;
	int ignore_case;
} git_sparse;

typedef enum {
	GIT_SPARSE_UNCHECKED = -2,
	GIT_SPARSE_NOTFOUND = -1,
	GIT_SPARSE_NOCHECKOUT = 0,
	GIT_SPARSE_CHECKOUT = 1,
} git_sparse_status;

extern int git_sparse__init(git_repository *repo, git_sparse *ign);

extern void git_sparse__free(git_sparse *sparse);

extern int git_sparse__lookup(
		git_sparse_status* checkout,
		git_sparse *sparse,
		const char* pathname,
		git_dir_flag dir_flag);

#endif
