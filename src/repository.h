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
	git_otype type;
};

struct git_repository {
	git_odb *db;
	git_hashtable *objects;
};

int git_repository__insert(git_repository *repo, git_repository_object *obj);
git_repository_object *git_repository__lookup(git_repository *repo, const git_oid *id);

#endif
