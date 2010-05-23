#ifndef INCLUDE_revwalk_h__
#define INCLUDE_revwalk_h__

#include "git/common.h"
#include "git/revwalk.h"

#define GIT_REVPOOL_SORT_NONE (0)
#define GIT_REVPOOL_SORT_TOPO (1 << 0)
#define GIT_REVPOOL_SORT_TIME (1 << 1)
#define GIT_REVPOOL_SORT_REVERSE (1 << 2)

struct git_revpool {
	git_odb *db;

    git_commit_list iterator;
    git_commit *(*next_commit)(git_commit_list *);

    git_commit_list roots;
    git_revpool_table *commits;

    unsigned walking:1;
    unsigned char sorting;
};

#endif /* INCLUDE_revwalk_h__ */
