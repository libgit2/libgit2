#ifndef _INCLUDE_git_indexer_h__
#define _INCLUDE_git_indexer_h__

#include "git2/common.h"

typedef struct git_indexer_stats {
	unsigned int total;
	unsigned int parsed;
} git_indexer_stats;


typedef struct git_pack_indexer git_pack_indexer;

GIT_EXTERN(int) git_pack_indexer_new(git_pack_indexer **out, const char *packname);
GIT_EXTERN(int) git_pack_indexer_run(git_pack_indexer *idx, int (*cb)(const git_indexer_stats *, void *), void *data);
GIT_EXTERN(void) git_pack_indexer_free(git_pack_indexer *idx);


#endif
