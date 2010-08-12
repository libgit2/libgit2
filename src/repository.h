#ifndef INCLUDE_repository_h__
#define INCLUDE_repository_h__

#include "git/common.h"
#include "git/oid.h"
#include "git/odb.h"
#include "git/repository.h"

#include "hashtable.h"

struct git_repository_object {
	git_oid id;
	git_repository *repo;
	git_obj dbo;
	int dbo_open;
};

struct git_repository {
	git_odb *db;
	git_hashtable *objects;
};


int git_repository__open_dbo(git_repository_object *object);
void git_repository__close_dbo(git_repository_object *object);

#endif
