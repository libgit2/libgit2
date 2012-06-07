/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__

#include "git2/commit.h"
#include "tree.h"
#include "repository.h"
#include "vector.h"

#include <time.h>

struct git_commit {
	git_object object;

	git_vector parent_oids;
	git_oid tree_oid;

	git_signature *author;
	git_signature *committer;

	char *message_encoding;
	char *message;
};

void git_commit__free(git_commit *c);
int git_commit__parse(git_commit *commit, git_odb_object *obj);

int git_commit__parse_buffer(git_commit *commit, const void *data, size_t len);
#endif
