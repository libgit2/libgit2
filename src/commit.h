#ifndef INCLUDE_commit_h__
#define INCLUDE_commit_h__
#include "git/commit.h"

#include <time.h>

struct git_commit {
	git_oid id;
	time_t commit_time;
	unsigned parsed:1,
	         flags:26;
};

#endif
