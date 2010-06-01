#ifndef INCLUDE_revwalk_h__
#define INCLUDE_revwalk_h__

#include "git/common.h"
#include "git/revwalk.h"

struct git_revpool {
	git_odb *db;

	git_commit_list iterator;
	git_commit *(*next_commit)(git_commit_list *);

	git_commit_list roots;
	git_revpool_table *commits;

	unsigned walking:1;
	unsigned int sorting;
};

void gitrp__prepare_walk(git_revpool *pool);
int gitrp__enroot(git_revpool *pool, git_commit *commit);

#endif /* INCLUDE_revwalk_h__ */
