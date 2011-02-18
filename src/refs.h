#ifndef INCLUDE_refs_h__
#define INCLUDE_refs_h__

#include "common.h"
#include "git2/oid.h"
#include "git2/refs.h"
#include "hashtable.h"

#define GIT_REFS_DIR "refs/"
#define GIT_REFS_HEADS_DIR GIT_REFS_DIR "heads/"
#define GIT_REFS_TAGS_DIR GIT_REFS_DIR "tags/"
#define GIT_REFS_REMOTES_DIR GIT_REFS_DIR "remotes/"

#define GIT_SYMREF "ref: "
#define GIT_PACKEDREFS_FILE "packed-refs"
#define GIT_PACKEDREFS_HEADER "# pack-refs with: peeled "
#define MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH 100

struct git_reference {
	git_repository *owner;
	char *name;
	unsigned int type;
};

typedef struct {
	git_hashtable *packed_refs;
	git_hashtable *loose_refs;

	unsigned pack_loaded:1;
} git_refcache;


void git_repository__refcache_free(git_refcache *refs);
int git_repository__refcache_init(git_refcache *refs);

int git_reference__normalize_name(char *buffer_out, const char *name);
int git_reference__normalize_name_oid(char *buffer_out, const char *name);

#endif
