#ifndef _INCLUDE_git_indexer_h__
#define _INCLUDE_git_indexer_h__

#include "git2/common.h"
#include "git2/oid.h"

typedef struct git_indexer_stats {
	unsigned int total;
	unsigned int processed;
} git_indexer_stats;


typedef struct git_indexer git_indexer;

GIT_EXTERN(int) git_indexer_new(git_indexer **out, const char *packname);
GIT_EXTERN(int) git_indexer_run(git_indexer *idx, git_indexer_stats *stats);
GIT_EXTERN(const git_oid *) git_indexer_result(git_indexer *idx);
GIT_EXTERN(void) git_indexer_free(git_indexer *idx);


#endif
