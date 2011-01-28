#ifndef INCLUDE_refs_h__
#define INCLUDE_refs_h__

#include "common.h"
#include "git2/oid.h"
#include "hashtable.h"

#define GIT_REFS_DIR "refs/"
#define GIT_REFS_HEADS_DIR GIT_REFS_DIR "heads/"
#define GIT_REFS_TAGS_DIR GIT_REFS_DIR "tags/"

#define GIT_SYMREF "ref: "
#define GIT_PACKEDREFS_FILE "packed-refs"
#define GIT_PACKEDREFS_HEADER "# pack-refs with: peeled \n"
#define MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH 100

struct git_reference {
	git_repository *owner;
	git_rtype type;
	char *name;

	unsigned packed:1,
			 modified:1;

	union {
		char *ref;
		git_oid oid;
	} target;
};

typedef struct {
	git_hashtable *cache;
	unsigned pack_loaded:1;
} git_refcache;


void git_repository__refcache_free(git_refcache *refs);
int git_repository__refcache_init(git_refcache *refs);

#endif
