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

	struct {
		void *write_ptr;
		size_t ptr_size;
		size_t written_bytes;
	} writeback;

	int dbo_open:1, out_of_sync:1;
};

struct git_repository {
	git_odb *db;
	git_hashtable *objects;
};


int git_repository__dbo_open(git_repository_object *object);
void git_repository__dbo_close(git_repository_object *object);
void git_repository__dbo_prepare_write(git_repository_object *object);
int git_repository__dbo_write(git_repository_object *object, const void *bytes, size_t len);
int git_repository__dbo_writeback(git_repository_object *object);

#endif
