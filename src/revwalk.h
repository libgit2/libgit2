#ifndef INCLUDE_revwalk_h__
#define INCLUDE_revwalk_h__

#include "git/common.h"
#include "git/revwalk.h"

struct git_revpool {
	git_odb *db;
    git_commit_list iterator;
    git_commit_list roots;

    unsigned walking:1,
             topological_sort:1;
};

#endif /* INCLUDE_revwalk_h__ */
