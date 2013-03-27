/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_graph_h__
#define INCLUDE_graph_h__

#include "common.h"

typedef struct {
	git_oid oid;
	uint32_t time;
} git_graph_commit;

struct git_graph_commit_list {
	git_vector commits;
};

#endif /* INCLUDE_graph_h__ */
