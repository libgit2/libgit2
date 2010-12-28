#ifndef INCLUDE_refs_h__
#define INCLUDE_refs_h__

#include "common.h"
#include "git2/oid.h"

#include "hashtable.h"

struct git_reference {
	git_rtype type;
	char *name;
};

typedef struct {
	git_hashtable *references;
	
	unsigned is_loaded:1;
	unsigned is_busy:1;
} reference_database;


reference_database *reference_database__alloc();
void reference_database__free(reference_database* ref_database);

#endif