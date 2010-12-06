#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__

#include "git2/commit.h"
#include "tree.h"
#include "repository.h"
#include "vector.h"

#include <time.h>

struct git_commit {
	git_object object;

	time_t commit_time;
	git_vector parents;

	git_tree *tree;
	git_person *author;
	git_person *committer;

	char *message;
	char *message_short;

	unsigned full_parse:1;
};

void git_commit__free(git_commit *c);
int git_commit__parse(git_commit *commit);
int git_commit__parse_full(git_commit *commit);

int git_commit__writeback(git_commit *commit, git_odb_source *src);

#endif
