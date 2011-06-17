#ifndef INCLUDE_branch_h__
#define INCLUDE_branch_h__

struct git_branch {
	char *remote; /* TODO: Make this a git_remote */
	char *merge;
};

#endif
