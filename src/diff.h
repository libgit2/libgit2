/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_diff_h__
#define INCLUDE_diff_h__

#include <stdio.h>
#include "vector.h"
#include "buffer.h"
#include "iterator.h"
#include "repository.h"

struct git_diff_list {
	git_repository   *repo;
	git_diff_options opts;
	git_vector       deltas;    /* vector of git_diff_file_delta */
	git_iterator_type_t old_src;
	git_iterator_type_t new_src;
};

#endif

