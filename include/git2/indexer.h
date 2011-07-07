#ifndef _INCLUDE_git_indexer_h__
#define _INCLUDE_git_indexer_h__

typedef struct git_pack_indexer {
	struct pack_file *pack;
	git_vector objects;
	git_vector deltas;
	struct stat st;
} git_pack_indexer;

GIT_EXTERN(int) git_pack_indexer_new(git_pack_indexer **out, const char *packname);
GIT_EXTERN(void) git_pack_indexer_free(git_pack_indexer *idx)


#endif
