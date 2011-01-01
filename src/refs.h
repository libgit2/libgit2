#ifndef INCLUDE_refs_h__
#define INCLUDE_refs_h__

#include "common.h"
#include "git2/oid.h"
#include "hashtable.h"

#define GIT_SYMREF "ref:"
#define MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH 100

struct git_reference {
	git_rtype type;
	char *name;

	unsigned is_packed:1;
};

struct git_reference_object_id {
	git_reference base;

	git_oid id;
};

struct git_reference_symbolic {
	git_reference base;

	git_reference *target;
};

typedef struct {
	git_hashtable *references;
	
	unsigned is_fully_loaded:1;
	unsigned have_packed_refs_been_parsed:1;
	unsigned is_busy:1;
} git_reference_database;

git_reference_database *git_reference_database__alloc();
void git_reference_database__free(git_reference_database *ref_database);
int git_reference_lookup(git_reference **reference_out, git_reference_database *ref_database, const char *name, const char *path_repository, int *nesting_level);

#endif